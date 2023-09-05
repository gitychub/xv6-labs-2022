#include "kernel/param.h"
#include "kernel/types.h"
#include "user.h"

#define MAX_ARGLEN 1024

void xarg(int argc, char **args, int ind) {
  int pid = fork();
  if (pid < 0) {
    fprintf(2, "[xargs] fork failed\n");
    exit(1);
  } else if (pid > 0) {
    wait(0);
  } else {
    exec(args[0], args);
  }

  for (int i = argc; i < ind; i++) {
    free(args[i]);
  }
}

int main(int argc, char *argv[]) {
  char *args[MAXARG];
  // throw away the first argument of args `xargs`
  memmove(args, ++argv, (--argc) * sizeof(char *));

  int len = 0, ind = argc;
  char arg[MAX_ARGLEN];
  while (read(0, &arg[len], sizeof(char)) == sizeof(char)) {
    if (arg[len] == ' ' || arg[len] == '\n' || arg[len] == '\r') {
      args[ind] = (char *)malloc(len + 1);
      memmove(args[ind], arg, len);
      args[ind++][len] = '\0';
      
      if (arg[len] != ' ') {
        xarg(argc, args, ind);
        ind = argc;
      }
      len = 0;
    } else {
      len++;
    }
  }
  exit(0);
}
