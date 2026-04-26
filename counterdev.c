#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <linux/atmioc.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>

#include <linux/types.h>

#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/proc_fs.h>

#include <asm/errno.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define PROCFS_NAME "counterdevstats"



#define DEVICE_NAME "counterdev"
#define COUNTER_LEN 4
#define COUNTED_LEN 21

static struct kobject *sysfs_module;

static uint64_t num_counter = 0;
static uint64_t num_counted = 0;

static int major;

static struct class *cls;

static char counter_str[COUNTER_LEN + 1];
static char counted_str[COUNTED_LEN + 1];

// 0 - ADD
// 1 - MUL
// 2 - DIV

static int op_type = 0;

enum {
    CDEV_NOT_USED,
    CDEV_EXCLUSIVE_OPEN
};

static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static struct proc_dir_entry *stats_proc_file;

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);
static ssize_t procfile_read(struct file *, char __user *, size_t, loff_t *);
static int procfs_open(struct inode *, struct file *);
static int procfs_close(struct inode *, struct file *);

static struct file_operations counterdev_fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

#ifdef HAVE_PROC_OPS

static struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
    .proc_open = procfs_open,
    .proc_release = procfs_close,
};

#else

static const struct  file_operations proc_file_fops = {
    .read = procfile_read,
    .open = procfs_open,
    .release = procfs_close,

};


#endif

static ssize_t op_type_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", op_type);
}

static ssize_t op_type_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    sscanf(buf, "%d", &op_type);

    return count;
}

static struct kobj_attribute op_type_attribute = __ATTR(op_type, 0660, op_type_show, op_type_store);

static int __init counterdev_init(void) {
    int error = 0;
    pr_info("Device driver registration initiated\n");

    #ifdef HAVE_PROC_OPS
    pr_info("Procops available\n");
    #else
    pr_info("Procops not available failing to file operations\n");
    #endif

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

    stats_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);

    if (stats_proc_file == NULL) {
        pr_alert("Error: Could not initialise /proc/%s\n", PROCFS_NAME);

        return -ENOMEM;
    }

    proc_set_user(stats_proc_file, GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);

    pr_info("/proc/%s created\n", PROCFS_NAME);

    // initialise sysfs file
    sysfs_module = kobject_create_and_add("op_type", kernel_kobj);
    if (!sysfs_module) {
        return -ENOMEM;
    }

    error = sysfs_create_file(sysfs_module, &op_type_attribute.attr);

    if (error) {
        kobject_put(sysfs_module);
        pr_info("Failed to create op_type variable file in /sys/kernel/op_type\n");
        return error;
    }

    return 0;
}


static void __exit counterdev_exit(void) {
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("Device driver unregistered\n");

    proc_remove(stats_proc_file);
    pr_info("/proc/%s removed\n", PROCFS_NAME);

    kobject_put(sysfs_module);
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
        if (op_type == 0) {
            num_counter += delta;
        } else {
            num_counter *= delta;
        }
        if (num_counter > 999) {
            pr_info("Counter exceeded 999, wrapping over\n");
            num_counter = num_counter % 1000;
        }
        num_counted += 1;
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
    sprintf(counter_str, "%lld\n", num_counter);
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

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t len, loff_t *offset) {
    int lenc = 0;
    ssize_t ret = 0;
    memset(counted_str, '\0', COUNTED_LEN + 1);
    sprintf(counted_str, "%lld\n", num_counted);
    lenc = sizeof(counted_str);

    ret = lenc;

    if (*offset >= lenc || copy_to_user(buffer, counted_str, lenc)) {
        pr_info("Either reading finished or copy_to_user failed");
        ret = 0;
    } else {
        *offset += lenc;
    }

    return ret;
}

static int procfs_open(struct inode *inode, struct file *file) {
    pr_info("proc file openned\n");
    return 0;
}

static int procfs_close(struct inode *inode, struct file *file) {
    pr_info("proc file closed\n");
    return 0;
}



module_init(counterdev_init);
module_exit(counterdev_exit);

MODULE_LICENSE("GPL");
