#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"

#define MAXARGLEN 10

// 从标准输入获取参数
void getargs(char args[MAXARG][MAXARGLEN]) {
  int i = 0;
  int j = 0;
  char c;

  while (read(0, &c, 1) > 0 && i < MAXARG) {
    if (c == ' ' || c == '\n') {
      if (j > 0) {  // 只有在有字符时才终止当前参数
        args[i][j] = 0;
        i++;
        j = 0;
      }
    } else {
      if (j < MAXARGLEN) {
        if (j < MAXARGLEN - 1) {  // 确保不会溢出
          args[i][j++] = c;
        } else {
          fprintf(2, "Warning: Argument too long.\n");
          j = MAXARGLEN - 1;  // 防止溢出
        }
      }
    }
  }

  if (j > 0) {  // 如果最后一个参数没有被终止
    args[i][j] = 0;
    i++;
  }

  args[i][0] = 0;  // 终止参数列表
}

int main(int argc, char *argv[]) {
  char prev_args[MAXARG][MAXARGLEN];
  char *run_args[MAXARG];

  // 输入参数数量检查
  if (argc == 1) {
    fprintf(2, "Error: xargs needs args.\n");
    exit(1);
  }

  getargs(prev_args);

  // 复制argv
  for (int i = 0; i < argc - 1; i++) {
    run_args[i] = argv[i + 1];
  }

  int i = argc - 1;
  int j = 0;

  // 将从stdin获取的args添加到run_args
  while (prev_args[j][0] != 0) {
    if (i >= MAXARG) {
      fprintf(2, "Error: Too many arguments.\n");
      exit(1);
    }
    run_args[i++] = prev_args[j++];
  }

  run_args[i] = 0;

  if (fork() == 0) {
    if (exec(run_args[0], run_args) < 0) {
      fprintf(2, "xargs: exec %s failed\n", run_args[0]);
      exit(1);
    }
  } else {
    wait(0);
  }

  exit(0);
}