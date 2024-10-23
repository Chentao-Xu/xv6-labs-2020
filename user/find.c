#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

// 获取文件名的简化版（去掉路径中的目录部分）
char *fmtname(char *path) {
  // static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return non-blank-padded name.
  return p;
}

// 在指定路径下查找目标文件或目录
void find(char *path, char *target) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // 如果当前路径的文件名与目标文件名相同，则打印路径
  if (strcmp(fmtname(path), target) == 0) {
    printf("%s\n", path);
  }

  if (st.type == T_DIR) {
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      printf("find: path too long\n");
      close(fd);
      return;
    }

    // 将当前路径复制到缓冲区，并添加斜杠 '/' 准备拼接子目录名或文件名
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0 ||
          de.inum == 0) {
        continue;
      }
      // 将目录项的名称拼接到当前路径中
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      
      find(buf, target);
    }
  }
  close(fd);
  return;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "find needs two args\n");
    exit(0);
  }

  find(argv[1], argv[2]);
  exit(0);
}