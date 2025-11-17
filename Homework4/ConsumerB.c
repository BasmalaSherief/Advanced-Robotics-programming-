#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct DataCoord {
    int x_coor;
    int y_coor;
    char command;
};

int main()
{
    int fd;
    struct DataCoord msg;

    // Open FIFO for reading
    fd = open("/tmp/my_drawing_pipe", O_RDONLY);
    if (fd == -1) {
        perror("ConsumerB: open failed");
        exit(1);
    }

    // Start ncurses
    initscr();
    cbreak();
    noecho();

    mvprintw(0, 0, "Consumer B: Mirroring Producer A.");
    refresh();

    while (1) {
        ssize_t n = read(fd, &msg, sizeof(msg));

        if (n <= 0) {
            break; // EOF or error
        }

        if (msg.command == 'q') {
            break; // Quit
        }

        mvaddch(msg.y_coor, msg.x_coor, '*');
        refresh();
    }

    close(fd);
    endwin();
    printf("Consumer B terminated.\n");
    return 0;
}
