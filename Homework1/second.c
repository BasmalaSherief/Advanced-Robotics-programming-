#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

int main(void) {
    const char *fifo1 = "/tmp/myfifo";
    const char *fifo2 = "/tmp/myfifo2";

    mkfifo(fifo1, 0666);
    mkfifo(fifo2, 0666);

    char str1[80], str2[80];
    int n1, n2;
    double mean;

    printf("Process 2 ready.\n");

    while (1) {
        /* --- wait for process 1 to write something --- */
        int fd_in = open(fifo1, O_RDONLY);
        if (fd_in < 0) {
            perror("open fifo1");
            exit(EXIT_FAILURE);
        }

        ssize_t bytes = read(fd_in, str1, sizeof(str1));
        close(fd_in);

        if (bytes <= 0) continue;  // nothing read, loop again

        /* check for quit */
        if (str1[0] == 'q') {
            int fd_out = open(fifo2, O_WRONLY);
            write(fd_out, "q", 2);
            close(fd_out);
            printf("Quit signal received → exiting.\n");
            break;
        }

        /* parse numbers */
        if (sscanf(str1, "%d,%d", &n1, &n2) == 2) {
            mean = (n1 + n2) / 2.0;
            printf("mean value is: %.2f, sum is: %d\n", mean, n1 + n2);

            /* open fifo2 only when we have data to send */
            int fd_out = open(fifo2, O_WRONLY);
            snprintf(str2, sizeof(str2), "%f,%f", (double)n1, (double)n2);
            write(fd_out, str2, strlen(str2) + 1);
            close(fd_out);
        } else {
            printf("Invalid input: %s\n", str1);
        }
    }
    return 0;
}
