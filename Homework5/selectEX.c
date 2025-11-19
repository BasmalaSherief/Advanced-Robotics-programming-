#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>

// Buffer size for messages
#define BUF_SIZE 64

// Function for Producer 1
void producer1(int write_fd) {
    char msg[BUF_SIZE];
    int counter = 0;
    
    // Random seed based on PID
    srand(getpid()); 

    while (1) {
        // Create a tagged message
        snprintf(msg, BUF_SIZE, "[P1] Message %d", counter++);
        
        // Write to pipe
        if (write(write_fd, msg, strlen(msg) + 1) == -1) {
            perror("P1 write error");
            exit(1);
        }

        // Sleep for a random interval 
        // usleep takes microseconds
        int sleep_time = 500000 + (rand() % 1000000); 
        usleep(sleep_time); 
    }
}

// Function for Producer 2
void producer2(int write_fd) {
    char msg[BUF_SIZE];
    int counter = 0;
    
    srand(getpid());

    while (1) {
        snprintf(msg, BUF_SIZE, "<P2> Data packet %d", counter++);
        
        if (write(write_fd, msg, strlen(msg) + 1) == -1) {
            perror("P2 write error");
            exit(1);
        }

        // P2 sleeps faster to create different cycles
        int sleep_time = 200000 + (rand() % 600000);
        usleep(sleep_time);
    }
}

// Helper to read from a ready pipe
int read_from_pipe(int fd, const char* source_name) {
    char buffer[BUF_SIZE];
    int nbytes = read(fd, buffer, BUF_SIZE);
    
    if (nbytes > 0) {
        printf("Consumer received from %s: %s\n", source_name, buffer);
        return 1; 
    } else if (nbytes == 0) {
        printf("Consumer: %s closed connection.\n", source_name);
        return 0; 
    } else {
        perror("read error");
        return -1; 
    }
}

int main() {
    int pipe1[2], pipe2[2];
    pid_t pid1, pid2;

    // 1. Create Pipes
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("pipe creation failed");
        exit(1);
    }

    // 2. Fork Producer 1
    pid1 = fork();
    if (pid1 == 0) {
        // Child P1
        close(pipe1[0]); // Close read end
        close(pipe2[0]); // Close P2's pipe completely
        close(pipe2[1]);
        producer1(pipe1[1]);
        exit(0);
    }

    // 3. Fork Producer 2
    pid2 = fork();
    if (pid2 == 0) {
        // Child P2
        close(pipe2[0]); // Close read end
        close(pipe1[0]); // Close P1's pipe completely
        close(pipe1[1]);
        producer2(pipe2[1]);
        exit(0);
    }

    // 4. Consumer (Parent Process) Logic
    close(pipe1[1]); // Close write ends
    close(pipe2[1]);

    int fd1 = pipe1[0];
    int fd2 = pipe2[0];
    int max_fd = (fd1 > fd2 ? fd1 : fd2) + 1;
    
    fd_set read_fds;
    struct timeval timeout;
    
    // Seed random for the fair choice logic
    srand(time(NULL)); 

    printf("Consumer started. Monitoring FD %d (P1) and FD %d (P2)...\n", fd1, fd2);

    int active_producers = 2;

    while (active_producers > 0) {
        FD_ZERO(&read_fds);
        FD_SET(fd1, &read_fds);
        FD_SET(fd2, &read_fds);

        // Set a timeout
        // If no data arrives in 2s, do something else
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        // --- THE SELECT CALL ---
        int activity = select(max_fd, &read_fds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
            break;
        }

        if (activity == 0) {
            printf("Consumer: Idle... waiting for data.\n");
            continue;
        }

        // Check which FDs are ready
        int p1_ready = FD_ISSET(fd1, &read_fds);
        int p2_ready = FD_ISSET(fd2, &read_fds);

        // --- NON-DETERMINISM / FAIRNESS LOGIC ---
        // If both are ready, randomly decide which one to read first
        if (p1_ready && p2_ready) {
            if (rand() % 2 == 0) {
                // Read P1 then P2
                if (read_from_pipe(fd1, "Pipe 1") == 0) active_producers--;
                if (read_from_pipe(fd2, "Pipe 2") == 0) active_producers--;
            } else {
                // Read P2 then P1
                if (read_from_pipe(fd2, "Pipe 2") == 0) active_producers--;
                if (read_from_pipe(fd1, "Pipe 1") == 0) active_producers--;
            }
        }
        else if (p1_ready) {
            if (read_from_pipe(fd1, "Pipe 1") == 0) active_producers--;
        }
        else if (p2_ready) {
            if (read_from_pipe(fd2, "Pipe 2") == 0) active_producers--;
        }
    }

    printf("All producers finished. Consumer exiting.\n");
    
    // Cleanup
    close(fd1);
    close(fd2);
    wait(NULL);
    wait(NULL);

    return 0;
}