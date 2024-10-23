#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Sleep needs one arguments!\n");
    }
    int ticks = atoi(argv[1]);
    sleep(ticks);
    printf("(nothing happens for a little while)\n");
    exit(0);
}