#ifndef CHARDEV_H
#define CHARDEV

#include <linux/ioctl.h>

#define IOCTL_MAGIC '\x66'

#define IOCTL_VALSET _IOW(IOCTL_MAGIC, 0, uint64_t)

#define DEVICE_PATH "/dev/counterdev"

#endif