#define _XOPEN_SOURCE 700 // Required for sigaction features
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <ncurses.h>
#include <string.h>

#define NUM_WORKERS 3
#define TIMEOUT_SEC 4      // Die if silent for 4 seconds
#define LOG_FILE "watchdog.log"

// Global array to store PIDs of children
pid_t worker_pids[NUM_WORKERS];

// Global array to store last heartbeat time for each worker
// We use 'volatile' because it is modified in a signal handler
volatile time_t last_heartbeat[NUM_WORKERS];

// Helper function to get current time string
void get_time_str(char *buffer, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%H:%M:%S", t);
}

// --- SIGNAL HANDLER (Run by Watchdog) ---
// This runs whenever a worker sends SIGUSR1
void watchdog_handler(int sig, siginfo_t *info, void *context) {
    // 1. Find out WHO sent the signal (si_pid)
    pid_t sender_pid = info->si_pid;
    
    // 2. Update their timestamp
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (worker_pids[i] == sender_pid) {
            last_heartbeat[i] = time(NULL);
            
            // 3. Write to logfile (as requested)
            FILE *f = fopen(LOG_FILE, "a");
            if (f) {
                char time_str[20];
                get_time_str(time_str, sizeof(time_str));
                fprintf(f, "[%s] Received heartbeat from P%d (PID %d)\n", time_str, i+1, sender_pid);
                fclose(f);
            }
            break;
        }
    }
}

// --- WORKER PROCESS CODE ---
void run_worker(int id, pid_t watchdog_pid) {
    srand(getpid()); // Seed random number generator
    
    // P3 will be the "faulty" process for testing
    int cycles = 0;
    int is_faulty = (id == 2); // Worker index 2 (P3) is faulty

    while (1) {
        // 1. Send Signal to Watchdog
        kill(watchdog_pid, SIGUSR1);

        // 2. Simulate different periods (random sleep 0.5s - 1.5s)
        // using usleep (microseconds)
        int sleep_time = 500000 + (rand() % 1000000);
        sleep(sleep_time);

        // 3. Simulate Failure
        if (is_faulty) {
            cycles++;
            if (cycles >= 5) {
                // Stop sending signals (simulate freeze)
                // We enter an infinite loop without signaling
                while(1) sleep(1);
            }
        }
    }
}

// --- MAIN (MASTER / WATCHDOG) ---
int main() {
    // 1. Setup Signal Handler for SIGUSR1
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO; // Use extended info to get sender PID
    sa.sa_sigaction = watchdog_handler;
    sigaction(SIGUSR1, &sa, NULL);

    // 2. Clear Logfile
    FILE *f = fopen(LOG_FILE, "w");
    if (f) { fprintf(f, "--- Watchdog Started ---\n"); fclose(f); }

    // 3. Spawn Workers
    pid_t my_pid = getpid();
    time_t now = time(NULL);

    for (int i = 0; i < NUM_WORKERS; i++) {
        last_heartbeat[i] = now; // Initialize timers
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child Code
            run_worker(i, my_pid);
            exit(0);
        } else {
            // Parent stores the child PID
            worker_pids[i] = pid;
        }
    }

    // 4. Initialize Ncurses UI
    initscr();
    cbreak();
    noecho();
    curs_set(0); // Hide cursor

    // 5. Watchdog Loop
    while (1) {
        clear();
        mvprintw(0, 2, "WATCHDOG MONITOR (Press Ctrl+C to quit)");
        mvprintw(1, 2, "---------------------------------------");
        
        now = time(NULL);
        int alert = 0;

        for (int i = 0; i < NUM_WORKERS; i++) {
            double diff = difftime(now, last_heartbeat[i]);
            
            mvprintw(3 + i, 2, "Process P%d (PID %d): Last seen %.0f sec ago", 
                     i+1, worker_pids[i], diff);

            if (diff > TIMEOUT_SEC) {
                mvprintw(3 + i, 40, "[!!! TIMEOUT !!!]");
                alert = 1;
                
                // Log the failure
                FILE *f = fopen(LOG_FILE, "a");
                if (f) {
                    fprintf(f, "[ALERT] P%d (PID %d) died! Terminating all.\n", i+1, worker_pids[i]);
                    fclose(f);
                }
            } else {
                mvprintw(3 + i, 40, "[ OK ]");
            }
        }
        refresh();

        // 6. Handle Termination
        if (alert) {
            mvprintw(8, 2, "SYSTEM FAILURE DETECTED. TERMINATING...");
            refresh();
            sleep(2); // Show message briefly
            
            // Kill all children
            for (int i = 0; i < NUM_WORKERS; i++) {
                kill(worker_pids[i], SIGKILL);
            }
            break; // Exit loop
        }

        // Small sleep to prevent high CPU usage
        sleep(100000); // 0.1s
    }

    endwin(); // Close ncurses
    printf("Watchdog terminated safely. Check %s for details.\n", LOG_FILE);
    return 0;
}