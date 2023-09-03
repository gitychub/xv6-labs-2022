#include "kernel/types.h"
#include "user.h"

int main() {
  int p[2];
  if (pipe(p) < 0) {
    fprintf(2, "[pingpong] pipe failed\n");
    exit(1);
  }

  int pid = fork();
  char c = 'a';
  if (pid < 0) {
    fprintf(2, "[pingpong] fork failed\n");
    close(p[0]);
    close(p[1]);
    exit(1);
  } else if (pid == 0) {
    read(p[0], &c, sizeof(c));
    close(p[0]);
    printf("%d: received ping\n", getpid());
    write(p[1], &c, sizeof(c));
    close(p[1]);
    exit(0);
  } else {
    write(p[1], &c, sizeof(c));
    close(p[1]);
    read(p[0], &c, sizeof(c));
    close(p[0]);
    printf("%d: received pong\n", getpid());
    wait(0);
  }
  exit(0);
}
