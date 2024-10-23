#include "kernel/types.h"
#include "user.h"

#define MAX_NUM 35

void prime(int *prev_pfd) {
  close(prev_pfd[1]);  // 关闭写端

  int buf_prime;
  int buf;

  if (read(prev_pfd[0], &buf_prime, 4)) {
    int pfd[2];
    if (pipe(pfd) == -1) {
      printf("Failed to create a pipe\n");
      exit(1);
    }

    if (fork() == 0) {
      prime(pfd);  // 递归调用
    } else {
      close(pfd[0]);  // 关闭读端

      printf("prime %d\n", buf_prime);

      while (read(prev_pfd[0], &buf, 4)) {
        if (buf % buf_prime) {
          if (write(pfd[1], &buf, 4) == -1) {
            printf("Failed to write to pipe\n");
            exit(1);
          }
        }
      }
      close(pfd[1]);  // 关闭写端

      wait(0);
    }
  }

  close(prev_pfd[0]);  // 关闭读端
}

int main(int argc, char *argv[]) {
  int pfd[2];
  int max = MAX_NUM;

  if (pipe(pfd) == -1) {
    printf("Failed to create a pipe\n");
    exit(1);
  }

  if (fork() == 0) {
    prime(pfd);
  } else {
    close(pfd[0]);
    for (int i = 2; i <= max; i++) {
      if (write(pfd[1], &i, 4) == -1) {
        printf("Failed to write to pipe\n");
        exit(1);
      }
    }
    close(pfd[1]);

    wait(0); // 等待子进程
  }
  exit(0);
}