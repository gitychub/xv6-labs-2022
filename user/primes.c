#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

void sieve(int fd_in) {
  int prime;
  if (read(fd_in, &prime, sizeof(int)) != sizeof(int)) {
    return;
  }
  printf("prime %d\n", prime);

  int p[2];
  if (pipe(p) < 0) {
    fprintf(2, "[primes] pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "[primes] cannot fork process %d\n", getpid());
    close(fd_in);
    close(p[0]);
    close(p[1]);
    exit(1);
  } else if (pid > 0) {
    close(p[0]);
    int num;
    while (read(fd_in, &num, sizeof(int))) {
      if (num % prime != 0) {
        write(p[1], &num, sizeof(int));
      }
    }
    close(fd_in);
    close(p[1]);
    wait(0);
  } else {
    close(fd_in);
    close(p[1]);
    sieve(p[0]);
    exit(0);
  }
  exit(0);
}

int main() {
  int p[2];
  if (pipe(p) < 0) {
    fprintf(2, "[primes] pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "[primes] cannot fork process %d\n", getpid());
    close(p[0]);
    close(p[1]);
    exit(1);
  } else if (pid > 0) {
    close(p[0]);
    for (int i = 2; i <= 35; i++) {
      write(p[1], &i, sizeof(int));
    }
    close(p[1]);
    wait(0);
  } else {
    close(p[1]);
    sieve(p[0]);
    exit(0);
  }
  exit(0);
}
