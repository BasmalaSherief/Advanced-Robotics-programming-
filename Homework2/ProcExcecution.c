#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int spawn(const char * program, char ** arg_list) {
  pid_t child_pid = fork();
  if (child_pid != 0)
    return child_pid;
  else {
    execvp (program, arg_list);
    perror("exec failed");
    return 1;
 }
}

int main() {
  char * arg_list1[] = {"first", NULL };
  char * arg_list2[] = {"second", NULL };
  pid_t pid1 = spawn("./first", arg_list1);
  pid_t pid2 = spawn("./second", arg_list2);
  printf ("Main program exiting...\n");
  return 0;
}
