#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include "counterdev.h"


int ioctl_reset(int fd, uint64_t *ioctl_value) {
    int ret_val;
    printf("Setting value: %lu\n", *ioctl_value);

    ret_val = ioctl(fd, IOCTL_VALSET, ioctl_value);

    if (ret_val < 0) {
        printf("ioctl failed: %d\n", ret_val);
    }

    return ret_val;
}

int main(int argc, char *argv[]) {
    int fd, ret_val;
    uint64_t ioctl_value = 0;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device file: %s error: %d\n", DEVICE_PATH, fd);
        exit(EXIT_FAILURE);
    }

    if (argc > 1) {
        printf("Using value: %s\n", argv[1]);
        sscanf(argv[1], "%lu", &ioctl_value);
        printf("Using value num: %lu\n", ioctl_value);
    }

    ret_val = ioctl_reset(fd, &ioctl_value);
    close(fd);

    if (ret_val) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
