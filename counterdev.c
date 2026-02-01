#include <linux/module.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <linux/atmioc.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>

#include <linux/types.h>

#include <linux/uaccess.h>
#include <linux/version.h>

#include <asm/errno.h>

#define DEVICE_NAME "counterdev"
#define COUNTER_LEN 4

static uint64_t num_counter = 0;

static int major;

static struct class *cls;

static char counter_str[COUNTER_LEN + 1];

enum {
    CDEV_NOT_USED,
    CDEV_EXCLUSIVE_OPEN
};

static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations counterdev_fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init counterdev_init(void) {
    pr_info("Device driver registration initiated\n");

    major = register_chrdev(0, DEVICE_NAME, &counterdev_fops);

    if (major < 0) {
        pr_alert("Registering char device file failed with %d\n", major);
        return major;
    }

    pr_info("Device driver has been assigned major number: %d.\n", major);

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
    #else
    cls = class_create(THIS_MODULE, DEVICE_NAME);
    #endif

    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return 0;
}


static void __exit counterdev_exit(void) {
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("Device driver unregistered\n");
}


static int device_open(struct inode *inode, struct file *file) {
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) {
        return -EBUSY;
    }

    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    atomic_set(&already_open, CDEV_NOT_USED);
    
    return 0;
}

static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off) {
    uint64_t delta = 0;
    int counter_size = len, ret = 0;

    if (counter_size >= COUNTER_LEN) {
        counter_size = COUNTER_LEN;
    }

    memset(counter_str, 0, COUNTER_LEN + 1);
    if (copy_from_user(counter_str, buff, counter_size)) {
        return -EFAULT;
    }

    counter_str[counter_size] = '\0';
    *off += counter_size;

    ret = kstrtoull(counter_str, 10, &delta);
    if (ret == 0) {
        pr_info("Parsed number successfully\n");
        num_counter += delta;
        if (num_counter > 999) {
            pr_info("Counter exceeded 999, wrapping over\n");
            num_counter = num_counter % 1000;
        }
    } else if (ret == -ERANGE) {
        pr_info("Value provided is out of range\n");
    } else if (ret == -EINVAL) {
        pr_info("Invalid input format");
    } else {
        pr_info("Error occured during conversion");
    }

    return counter_size;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t len, loff_t *offset) {
    int lenc = 0;
    ssize_t ret = 0;
    memset(counter_str, '\0', COUNTER_LEN + 1);
    sprintf(counter_str, "%lld", num_counter);
    lenc = sizeof(counter_str);

    ret = lenc;

    if (*offset >= lenc || copy_to_user(buffer, counter_str, lenc)) {
        pr_info("Either reading finished or copy_to_user failed");
        ret = 0;
    } else {
        *offset += lenc;
    }

    return ret;


}



module_init(counterdev_init);
module_exit(counterdev_exit);

MODULE_LICENSE("GPL");
