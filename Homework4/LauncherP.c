#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

int spawn(const char * program, char ** arg_list) 
{
    pid_t child_pid = fork();
    if (child_pid != 0)
        return child_pid;
    else 
    {
        execvp(program, arg_list);
        perror("exec failed");
        exit(1);
    }
}

int main()
{
    const char* fifo = "/tmp/my_drawing_pipe";
    unlink(fifo);

    if (mkfifo(fifo, 0666) == -1) {
        perror("mkfifo failed");
        exit(1);
    }

    sleep(1); // give time for FIFO to be ready

    char* argA[] = { "xterm", "-hold", "-e", "./ProducerA", NULL };
    char* argB[] = { "xterm", "-hold", "-e", "./ConsumerB", NULL };

    pid_t pidA = spawn("xterm", argA);
    pid_t pidB = spawn("xterm", argB);

    waitpid(pidA, NULL, 0);
    waitpid(pidB, NULL, 0);

    unlink(fifo);
    return 0;
}
