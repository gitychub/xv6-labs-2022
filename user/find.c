#include "kernel/types.h" 
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user.h"

// Find first character after last slash.
char * fmtname(char *path) {
  char *p;
  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  return p + 1;
}

void find(char *path, char *target) {
  int fd = open(path, 0);
  if (fd < 0) {
    fprintf(2, "[find] cannot open %s\n", path);
    exit(1);
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    fprintf(2, "[find] cannot stat %s\n", path);
    close(fd);
    exit(1);
  }

  switch (st.type) {
    case T_DEVICE:
    case T_FILE:
      if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path);
      }
      break;

    case T_DIR:
      char buf[512];
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("[find] path too long\n");
        break;
      }
      strcpy(buf, path);

      char *p = buf + strlen(buf);
      *p++ = '/';

      struct dirent de;
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum > 0 && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
          memmove(p, de.name, DIRSIZ);
          p[DIRSIZ] = 0;
          find(buf, target);
        }
      }
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "[find] usage: find dir target\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
