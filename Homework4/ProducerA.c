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
    int x, y, ch, fd;
    struct DataCoord msg;

    // Open FIFO for writing
    fd = open("/tmp/my_drawing_pipe", O_WRONLY);
    if (fd == -1) {
        perror("ProducerA: open failed");
        exit(1);
    }

    // Start ncurses
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    x = max_x / 2;
    y = max_y / 2;

    mvprintw(0, 0, "Producer A: Use arrows to move. Press q to quit.");
    mvaddch(y, x, '*');
    refresh();

    while (1) {
        ch = getch();

        switch (ch) {
            case 'q':
                msg.command = 'q';
                write(fd, &msg, sizeof(msg));
                endwin();
                close(fd);
                printf("Producer A terminated.\n");
                return 0;

            case KEY_UP:    if (y > 1) y--; break;
            case KEY_DOWN:  if (y < max_y - 1) y++; break;
            case KEY_LEFT:  if (x > 0) x--; break;
            case KEY_RIGHT: if (x < max_x - 1) x++; break;
            default:
                continue;
        }

        msg.x_coor = x;
        msg.y_coor = y;
        msg.command = 'M'; // move

        write(fd, &msg, sizeof(msg));

        mvaddch(y, x, '*');
        refresh();
    }

    return 0;
}
