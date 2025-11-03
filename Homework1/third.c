
#include <stdio.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <stdlib.h>
  
int main() {
    
    int fd;
    char *fifo2 = "/tmp/myfifo2";
    mkfifo(fifo2, 0666);

    char str[80];
    double v1, v2;

    while (1) {
        fd = open(fifo2, O_RDONLY);
        read(fd, str, sizeof(str));

        // Check for quit signal
        if (str[0] == 'q') {
            printf("Quit signal received. Exiting...\n");
            close(fd);
            exit(EXIT_SUCCESS);
        }

        // Correct parsing of doubles
        if (sscanf(str, "%lf,%lf", &v1, &v2) == 2) {
            printf("The two values are: %.2f and %.2f\n", v1, v2);
        } else {
            printf("Invalid input: %s\n", str);
        }

        close(fd);
    }
    return 0;
}
