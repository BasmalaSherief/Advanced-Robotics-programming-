
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

#define N_PROCESSES 5
#define FIFO_NAME "/tmp/watchdog_fifo"
#define WATCHDOG_TIMEOUT 3  

// Function for the Worker Processes
void worker_process(int id) {
    char id_char = 'A' + id;
    int fd;

    // Open FIFO for writing
    // We retry opening because the watchdog might need a millisecond to create it
    while ((fd = open(FIFO_NAME, O_WRONLY)) == -1) {
        usleep(10000); 
    }

    printf("[Worker %c] Started.\n", id_char);

    int cycles = 0;
    while (1) {
        // 1. Send Heartbeat
        if (write(fd, &id_char, 1) == -1) {
            perror("Worker write failed");
            exit(1);
        }

        // 2. Simulate work (Sleep)
        // Normal behavior: sleep 1 second (well within the 3s timeout)
        sleep(1);

        // --- SIMULATION OF FAILURE ---
        // Process 'A' (id 0) will simulate a crash/freeze after 3 cycles
        if (id == 0) {
            cycles++;
            if (cycles >= 3) {
                printf("\n!!! [Worker %c] is freezing (simulating crash)... !!!\n\n", id_char);
                sleep(10); // Sleep longer than the timeout!
            }
        }
    }
    close(fd);
    exit(0);
}

// Function for the Watchdog Process
void watchdog_process() {
    time_t last_seen[N_PROCESSES];
    int fd;
    char buf;
    int i;

    // 1. Initialize timestamps to current time (give them a fair start)
    time_t now = time(NULL);
    for (i = 0; i < N_PROCESSES; i++) {
        last_seen[i] = now;
    }

    // 2. Open FIFO in NON-BLOCKING mode
    // This is crucial. If we use standard blocking read, the watchdog
    // would freeze if no one writes, and it couldn't check the timeouts.
    fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("Watchdog open failed");
        exit(1);
    }

    printf("[Watchdog] Monitoring %d processes...\n", N_PROCESSES);

    while (1) {
        // 3. Try to read ONE byte
        int n = read(fd, &buf, 1);

        if (n > 0) {
            // We received a heartbeat!
            int id = buf - 'A';
            if (id >= 0 && id < N_PROCESSES) {
                last_seen[id] = time(NULL); // Update timestamp
                // printf("[Watchdog] Reset timer for %c\n", buf); // Debug output
            }
        }

        // 4. Check for timeouts (The "Audit" phase)
        now = time(NULL);
        for (i = 0; i < N_PROCESSES; i++) {
            double diff = difftime(now, last_seen[i]);
            
            if (diff > WATCHDOG_TIMEOUT) {
                printf(">>> ALERT: Process %c has been silent for %.0f seconds! <<<\n", 
                       'A' + i, diff);
                
                // In a real system, we might kill/restart the process here.
                // For this demo, we just reset the timer so we don't spam stdout.
                last_seen[i] = now; 
            }
        }

        // 5. Sleep slightly to be "Fair" but not melt the CPU
        // "Not efficient" usually means busy-waiting, but a tiny sleep 
        // prevents the CPU from hitting 100% usage.
        usleep(100000); // 0.1 seconds
    }
    
    close(fd);
}

int main() {
    // Cleanup any old pipe
    unlink(FIFO_NAME);

    // Create Named Pipe
    if (mkfifo(FIFO_NAME, 0666) == -1) {
        perror("mkfifo failed");
        exit(1);
    }

    // Spawn N Workers
    for (int i = 0; i < N_PROCESSES; i++) {
        if (fork() == 0) {
            worker_process(i); // Child never returns
        }
    }

    // Parent becomes the Watchdog
    watchdog_process();

    // Cleanup (Unreachable code in this infinite loop example)
    unlink(FIFO_NAME);
    return 0;
}