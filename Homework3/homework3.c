#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // For fork, read, write, sleep, close
#include <sys/types.h>  // For mkfifo, open, pid_t
#include <sys/stat.h>   // For mkfifo
#include <fcntl.h>      // For open
#include <sys/wait.h>   // For wait
#include <errno.h>      // For errno
#include <string.h>     // For strerror

#define FIFO_NAME "/tmp/my_command_fifo"

// Define the message structure
struct message {
    char command;
    int number;
};

void process_I() {
    int fd;
    struct message msg;
    char cmd_char;
    int num;

    printf("Process I (PID %d) started. Waiting for writer...\n", getpid());
    
    fd = open(FIFO_NAME, O_WRONLY);
    if (fd == -1) {
        perror("Process I: open write");
        exit(1);
    }
    printf("Process I: FIFO opened. Enter command (A, B, q) and a number (e.g., 'A 123'):\n");

    while (1) {
        if (scanf(" %c %d", &cmd_char, &num) != 2) {
            while (getchar() != '\n');
            printf("Process I: Invalid input. Try again.\n");
            continue;
        }

        msg.command = cmd_char;
        msg.number = num;

        if (write(fd, &msg, sizeof(struct message)) == -1) {
            perror("Process I: write");
            break; 
        }

        if (msg.command == 'q') {
            break;
        }
    }

    printf("Process I: Sent 'q', terminating.\n");
    close(fd);
    exit(0);
}

void process_A() {
    int fd;
    struct message msg;
    int bytes_read;

    printf("Process A (PID %d) started. Waiting for reader...\n", getpid());

    fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        perror("Process A: open read");
        exit(1);
    }
    printf("Process A: FIFO opened.\n");

    while (1) {

        bytes_read = read(fd, &msg, sizeof(struct message));

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Process A: Writer closed FIFO.\n");
            } else {
                perror("Process A: read");
            }
            break; 
        }

        if (msg.command == 'q') {
            break;
        }

        if (msg.command == 'A') {
            printf("Process A: --- Received [A, %d]\n", msg.number);
            fflush(stdout); 
        }
    }

    printf("Process A: Received 'q' or EOF, terminating.\n");
    close(fd);
    exit(0);
}

void process_B() {
    int fd;
    struct message msg;
    int bytes_read;

    printf("Process B (PID %d) started. Waiting for reader...\n", getpid());

    fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        perror("Process B: open read");
        exit(1);
    }
    printf("Process B: FIFO opened.\n");

    while (1) {
        bytes_read = read(fd, &msg, sizeof(struct message));

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Process B: Writer closed FIFO.\n");
            } else {
                perror("Process B: read");
            }
            break;
        }

        if (msg.command == 'q') {
            break;
        }

        if (msg.command == 'B') {
            printf("Process B: +++ Received [B, %d]\n", msg.number);
            fflush(stdout); 
        }

    }

    printf("Process B: Received 'q' or EOF, terminating.\n");
    close(fd);
    exit(0);
}

int main() {
    pid_t pid_I, pid_A, pid_B;

    // Clean up any old FIFO file, ignoring error if it doesn't exist
    unlink(FIFO_NAME);

    if (mkfifo(FIFO_NAME, 0666) == -1) {
        if (errno != EEXIST) {
            perror("main: mkfifo");
            exit(1);
        }
    }
    printf("Parent: FIFO '%s' created.\n", FIFO_NAME);

    // Fork Process I
    pid_I = fork();
    if (pid_I == -1) {
        perror("main: fork I");
        exit(1);
    }
    if (pid_I == 0) {
        process_I();
    }

    // Fork Process A
    pid_A = fork();
    if (pid_A == -1) {
        perror("main: fork A");
        exit(1);
    }
    if (pid_A == 0) {
        process_A();
    }

    // Fork Process B
    pid_B = fork();
    if (pid_B == -1) {
        perror("main: fork B");
        exit(1);
    }
    if (pid_B == 0) {
        process_B();
    }

    // Parent process code
    printf("Parent (PID %d): Started processes I (PID %d), A (PID %d), B (PID %d)\n",
           getpid(), pid_I, pid_A, pid_B);
    
    // Wait for all three children to terminate
    wait(NULL);
    wait(NULL);
    wait(NULL);

    printf("Parent: All children terminated.\n");

    // Clean up the FIFO
    if (unlink(FIFO_NAME) == -1) {
        perror("main: unlink");
    }

    printf("Parent: FIFO unlinked. Exiting.\n");
    return 0;
}