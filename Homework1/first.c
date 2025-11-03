#include <stdio.h>
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <stdlib.h>

int main() {
    const char *fifo1 = "/tmp/myfifo";
    mkfifo(fifo1, 0666);

    char input[80];

    while (1) {
        int fd = open(fifo1, O_WRONLY);
        printf("Enter two integers separated by comma, or q to quit:\n");
        fflush(stdout);
        fgets(input, sizeof(input), stdin);
        write(fd, input, strlen(input) + 1);
        close(fd);

        if (input[0] == 'q') {
            exit(0);
        }
    }
}

