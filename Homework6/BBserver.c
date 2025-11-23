#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>

// Protocol: We send simple integers
#define ACK 1
#define READ_REQ 999

// --- WRITER PROCESS (W0 and W1) ---
void run_writer(int id, int fd_write, int fd_read) {
    srand(time(NULL) + id); // Unique seed
    int val, ack;

    while(1) {
        // Generate random integer (0-100)
        val = rand() % 100;

        // Send "Request to Write"
        // This will BLOCK here if the Server ignores us via select()
        write(fd_write, &val, sizeof(int));

        // Wait for Server Acknowledgment
        read(fd_read, &ack, sizeof(int));

        printf("[W%d] Successfully wrote: %d\n", id, val);
        
        usleep(500000); // Sleep 0.5s
    }
}

// --- READER PROCESS (R0 and R1) ---
void run_reader(int id, int fd_write, int fd_read) {
    int req = READ_REQ;
    int received_val;
    char filename[20];
    sprintf(filename, "log_R%d.txt", id);

    // Clear log file
    FILE *fp = fopen(filename, "w");
    if (fp) { fprintf(fp, "--- Log R%d ---\n", id); fclose(fp); }

    while(1) {
        // Send "Request to Read"
        // This BLOCKS if the Server logic decides value hasn't changed
        write(fd_write, &req, sizeof(int));

        // Read the Value
        read(fd_read, &received_val, sizeof(int));

        // Log it
        fp = fopen(filename, "a");
        if (fp) {
            fprintf(fp, "New Value in Cell %d: %d\n", id, received_val);
            fclose(fp);
        }
        printf("    [R%d] Logged new value: %d\n", id, received_val);
    }
}

// --- SERVER PROCESS (The Blackboard) ---
void run_server(int p_w0[2], int p_w1[2], int p_r0[2], int p_r1[2]) {
    int cell[2] = {0, 0};       // The Blackboard Memory
    int last_sent[2] = {-1, -1}; // To track changes for readers
    int val, temp;

    // Inputs (Server reads from these)
    int fd_w0_in = p_w0[0];
    int fd_w1_in = p_w1[0];
    int fd_r0_in = p_r0[0];
    int fd_r1_in = p_r1[0];

    // Outputs (Server writes to these)
    int fd_w0_out = p_w0[1];
    int fd_w1_out = p_w1[1];
    int fd_r0_out = p_r0[1];
    int fd_r1_out = p_r1[1];

    printf("[Server] Blackboard Started. State: [%d, %d]\n", cell[0], cell[1]);

    while(1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = 0;

        // ============================================================
        // THE LOGIC GUARDS
        // Only add FDs to the set if the Blackboard rules allow it!
        // ============================================================

        // Rule: W0 can only write if cell[0] <= cell[1] (or both 0)
        if (cell[0] <= cell[1]) {
            FD_SET(fd_w0_in, &readfds);
            if (fd_w0_in > max_fd) max_fd = fd_w0_in;
        }

        // Rule: W1 can only write if cell[1] <= cell[0] (or both 0)
        if (cell[1] <= cell[0]) {
            FD_SET(fd_w1_in, &readfds);
            if (fd_w1_in > max_fd) max_fd = fd_w1_in;
        }

        // Rule: R0 can only read if cell[0] has changed
        if (cell[0] != last_sent[0]) {
            FD_SET(fd_r0_in, &readfds);
            if (fd_r0_in > max_fd) max_fd = fd_r0_in;
        }

        // Rule: R1 can only read if cell[1] has changed
        if (cell[1] != last_sent[1]) {
            FD_SET(fd_r1_in, &readfds);
            if (fd_r1_in > max_fd) max_fd = fd_r1_in;
        }

        // ============================================================
        // SELECT (Non-Determinism)
        // ============================================================
        // This waits until an "Allowed" client sends a message
        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        // ============================================================
        // HANDLE REQUESTS (Atomicity)
        // ============================================================

        // --- Handle Writer 0 ---
        if (FD_ISSET(fd_w0_in, &readfds)) {
            read(fd_w0_in, &val, sizeof(int));
            cell[0] = val;
            val = ACK;
            write(fd_w0_out, &val, sizeof(int)); // Send Ack
            printf("[Server] W0 wrote %d. State: [%d, %d]\n", cell[0], cell[0], cell[1]);
        }

        // --- Handle Writer 1 ---
        if (FD_ISSET(fd_w1_in, &readfds)) {
            read(fd_w1_in, &val, sizeof(int));
            cell[1] = val;
            val = ACK;
            write(fd_w1_out, &val, sizeof(int)); // Send Ack
            printf("[Server] W1 wrote %d. State: [%d, %d]\n", cell[1], cell[0], cell[1]);
        }

        // --- Handle Reader 0 ---
        if (FD_ISSET(fd_r0_in, &readfds)) {
            read(fd_r0_in, &temp, sizeof(int)); // Consume request
            write(fd_r0_out, &cell[0], sizeof(int)); // Send Data
            last_sent[0] = cell[0]; // Mark as sent
        }

        // --- Handle Reader 1 ---
        if (FD_ISSET(fd_r1_in, &readfds)) {
            read(fd_r1_in, &temp, sizeof(int)); // Consume request
            write(fd_r1_out, &cell[1], sizeof(int)); // Send Data
            last_sent[1] = cell[1]; // Mark as sent
        }
    }
}

int main() {
    // 4 Pipe Pairs (Client->Server, Server->Client for each of the 4 clients)
    // Convention: p_client_req (Client writes, Server reads)
    //             p_client_res (Server writes, Client reads)
    int p_w0_req[2], p_w0_res[2];
    int p_w1_req[2], p_w1_res[2];
    int p_r0_req[2], p_r0_res[2];
    int p_r1_req[2], p_r1_res[2];

    pipe(p_w0_req); pipe(p_w0_res);
    pipe(p_w1_req); pipe(p_w1_res);
    pipe(p_r0_req); pipe(p_r0_res);
    pipe(p_r1_req); pipe(p_r1_res);

    // Spawn W0
    if (fork() == 0) {
        close(p_w0_req[0]); close(p_w0_res[1]); // Close unused ends
        run_writer(0, p_w0_req[1], p_w0_res[0]);
        exit(0);
    }
    // Spawn W1
    if (fork() == 0) {
        close(p_w1_req[0]); close(p_w1_res[1]);
        run_writer(1, p_w1_req[1], p_w1_res[0]);
        exit(0);
    }
    // Spawn R0
    if (fork() == 0) {
        close(p_r0_req[0]); close(p_r0_res[1]);
        run_reader(0, p_r0_req[1], p_r0_res[0]);
        exit(0);
    }
    // Spawn R1
    if (fork() == 0) {
        close(p_r1_req[0]); close(p_r1_res[1]);
        run_reader(1, p_r1_req[1], p_r1_res[0]);
        exit(0);
    }

    // Server (Parent)
    // Close client-side ends
    close(p_w0_req[1]); close(p_w0_res[0]);
    close(p_w1_req[1]); close(p_w1_res[0]);
    close(p_r0_req[1]); close(p_r0_res[0]);
    close(p_r1_req[1]); close(p_r1_res[0]);

    // Bundle pipes for cleaner function call
    int s_w0[] = {p_w0_req[0], p_w0_res[1]};
    int s_w1[] = {p_w1_req[0], p_w1_res[1]};
    int s_r0[] = {p_r0_req[0], p_r0_res[1]};
    int s_r1[] = {p_r1_req[0], p_r1_res[1]};

    run_server(s_w0, s_w1, s_r0, s_r1);

    return 0;
}