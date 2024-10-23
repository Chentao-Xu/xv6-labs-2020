#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  int p2c[2];
  int c2p[2];

  if (pipe(p2c) < 0 || pipe(c2p) < 0) {
    printf("pipe init faild!\n");
  }

  if (fork() == 0) {
    close(p2c[1]);
    close(c2p[0]);

    char buf;

    if (read(p2c[0], &buf, 1) < 0) {
      printf("child read from parent faild!\n");
    }

    int cpid = getpid();

    printf("%d: received ping\n", cpid);

    if (write(c2p[1], &buf, 1) < 0) {
      printf("child write to parent faild!\n");
    }

    close(c2p[1]);
    close(p2c[0]);
  } else {

    close(c2p[1]);
    close(p2c[0]);

    char buf;
    int ppid = getpid();

    if (write(p2c[1], &buf, 1) < 0) {
      printf("parent write to child faild\n");
    }

    if (read(c2p[0], &buf, 1) < 0) {
      printf("parent read from child faild\n");
    }

    printf("%d: received pong\n", ppid);

    close(p2c[1]);
    close(c2p[0]);
  }
  exit(0);
}