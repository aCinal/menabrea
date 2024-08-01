#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/fs.h>
#include <linux/wait.h>

#include <asm/uaccess.h>

static int __init dummy_init(void);
static void __exit dummy_exit(void);

static int dummy_open(struct inode *inode, struct file *filp);
static int dummy_release(struct inode *inode, struct file *filp);
static ssize_t dummy_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t dummy_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

#define DUMMY_MINOR 10

#define MOD_POWER_OF_TWO(val, pow)             ( (val) & ~(((size_t)(-1)) << (pow)) )
#define OFFSET_IDX(dev, off, rw)               MOD_POWER_OF_TWO((dev)->rw##idx + (off), ringsizelog)
#define ACTUAL_READ_IDX(dev)                   OFFSET_IDX((dev), 1, read)  /* Reads always occur one index ahead */
#define RINGBUFFER_WRITE_IDX_UPDATE(dev, inc)  (dev)->writeidx = OFFSET_IDX((dev), (inc), write)
#define RINGBUFFER_READ_IDX_UPDATE(dev, inc)   (dev)->readidx = OFFSET_IDX((dev), (inc), read)
#define RINGBUFFER_EMPTY(dev)                  (OFFSET_IDX((dev), 1, read) == (dev)->writeidx)
#define RINGBUFFER_FULL(dev)                   ((dev)->writeidx == (dev)->readidx)
#define RINGBUFFER_READ_ADDR(dev)              (&(dev)->buf[ACTUAL_READ_IDX(dev)])
#define RINGBUFFER_WRITE_ADDR(dev)             (&(dev)->buf[(dev)->writeidx])

static int ringsizelog = 10;
module_param(ringsizelog, int, S_IRUGO);

struct dummy_device {
    struct semaphore sem;      /* Mutual exclusion semaphore */
    struct cdev cdev;          /* Char device structure */
    wait_queue_head_t readq;   /* Read queue */
    wait_queue_head_t writeq;  /* Write queue */
    int writeidx;              /* Write index */
    int readidx;               /* Read index */
    int ringsize;              /* Ringbuffer size */
    char buf[0];               /* Data buffer */
};

struct dummy_device * dummy_device = NULL;
static struct class * dummy_class = NULL;
static struct device * dummy_struct_device = NULL;
static dev_t dummy_devt = 0;

static struct file_operations dummy_fops = {
    .owner = THIS_MODULE,
    .open = dummy_open,
    .release = dummy_release,
    .read = dummy_read,
    .write = dummy_write,
};

/**
 * @brief Register resources for the driver
 * @return Error code
 */
static int __init dummy_allocate_resources(void) {

    size_t ringsize;

    if (ringsizelog < 1 || ringsizelog >= sizeof(size_t) * 8) {

        printk(KERN_ERR "%s(): Invalid ringbuffer size (log 2): %d", \
            __FUNCTION__, ringsizelog);
        return -EINVAL;
    }

    ringsize = (1 << ringsizelog);

    /* Allocate the module's internal structure */
    printk(KERN_DEBUG "%s(): Allocating the device structure...\n", \
        __FUNCTION__);
    dummy_device = kmalloc(sizeof(struct dummy_device) + ringsize * sizeof(dummy_device->buf), GFP_KERNEL);
    if (NULL == dummy_device) {

        printk(KERN_ERR "%s(): Failed to allocate the internal device structure\n", \
            __FUNCTION__);
        return -ENOMEM;
    }
    /* Zero the structure */
    memset(dummy_device, 0, sizeof(struct dummy_device));
    printk(KERN_DEBUG "%s(): Device structure allocated successfully\n", __FUNCTION__);

    /* Configure the device structure */
    dummy_device->readidx = 0;
    /* Note that the write index is always ahead */
    dummy_device->writeidx = 1;
    dummy_device->ringsize = ringsize;

    init_waitqueue_head(&dummy_device->readq);
    init_waitqueue_head(&dummy_device->writeq);

    printk(KERN_INFO "%s(): Allocated ringbuffer of size %d\n", \
        __FUNCTION__, dummy_device->ringsize);

    /* Initialize the mutex */
    sema_init(&dummy_device->sem, 1);

    return 0;
}

/**
 * @brief Register the device with the kernel
 * @return Error code
 */
static int __init dummy_register_device_with_the_kernel(void) {

    /* Dynamically allocate a major number */
    int result = alloc_chrdev_region(&dummy_devt, DUMMY_MINOR, 1, "dummy-ringbuffer");
    if (result < 0) {

        printk(KERN_ERR "%s(): Failed to allocate a chrdev region: %d", \
            __FUNCTION__, result);
        return result;
    }

    /* Register a character device */
    cdev_init(&dummy_device->cdev, &dummy_fops);
    dummy_device->cdev.owner = THIS_MODULE;
    dummy_device->cdev.ops = &dummy_fops;
    result = cdev_add(&dummy_device->cdev, dummy_devt, 1);
    if (result) {

        printk(KERN_ERR "%s(): Failed to add the cdev structure: %d\n", \
            __FUNCTION__, result);
        return result;
    }

    return 0;
}

/**
 * @brief Add the device in userspace
 * @return Error code
 */
static int __init dummy_create_userspace_device_file(void) {

    /* Reach out to the udev (userspace /dev) daemon via a Netlink socket
     * for it to create a device node in /dev directory */
    dummy_class = class_create("dummy/ringbuffer");
    if (IS_ERR(dummy_class)) {

        return PTR_ERR(dummy_class);
    }

    dummy_struct_device = device_create(dummy_class, NULL, dummy_devt, NULL, "dummyring");
    if (IS_ERR(dummy_struct_device)) {

        return PTR_ERR(dummy_struct_device);
    }

    return 0;
}

/**
 * @brief Initialize the module
 */
static int __init dummy_init(void) {

    int result;

    /* Allocate driver resources */
    result = dummy_allocate_resources();
    if (result) {

        printk(KERN_ERR "Failed to allocate driver resources: %d\n", \
            result);
        goto err;
    }

    /* Register the device kernelside */
    result = dummy_register_device_with_the_kernel();
    if (result) {

        printk(KERN_ERR "Failed to register device with the kernel: %d\n", \
            result);
        goto err;
    }

    /* Create the device in userspace */
    result = dummy_create_userspace_device_file();
    if (result) {

        printk(KERN_ERR "Failed to create userspace device file: %d\n", \
            result);
        goto err;
    }

    return 0;

err:
    /* Clean up the device on error */
    dummy_exit();
    return result;
}

/**
 * @brief Undo dummy_create_userspace_device_file()
 */
static void __exit dummy_delete_userspace_device_file(void) {

    if (dummy_struct_device) {

        device_destroy(dummy_class, dummy_devt);
    }

    if (dummy_class) {

        class_destroy(dummy_class);
    }
}

/**
 * @brief Undo dummy_register_device_with_the_kernel()
 */
static void __exit dummy_deregister_device_from_the_kernel(void) {

    if (dummy_device) {

        cdev_del(&dummy_device->cdev);
    }

    if (dummy_devt) {

        unregister_chrdev_region(dummy_devt, 1);
    }
}

/**
 * @brief Undo dummy_allocate_resources()
 */
static void __exit dummy_deallocate_resources(void) {

    if (dummy_device) {

        kfree(dummy_device);
    }
}

/**
 * @brief Deinitialize the module
 */
static void __exit dummy_exit(void) {

    dummy_delete_userspace_device_file();
    dummy_deregister_device_from_the_kernel();
    dummy_deallocate_resources();
}

module_init(dummy_init);
module_exit(dummy_exit);

static int dummy_open(struct inode *inode, struct file *filp) {

    /* Offset backwards from the cdev structure contained in
     * inode to reference the wrapping dummy_device structure */
    struct dummy_device *dev = \
        container_of(inode->i_cdev, struct dummy_device, cdev);
    /* Store the reference for future use (note that filp is
     * associated with a descriptor, i.e. an open file, while
     * inode is associated with the file in the filesystem). */
    filp->private_data = dev;

    printk(KERN_INFO "%s (pid: %d) opened the dummy ringbuffer\n", \
        current->comm, current->pid);

    return 0;
}

static int dummy_release(struct inode *inode, struct file *filp) {

    (void) inode;
    (void) filp;
    printk(KERN_INFO "%s (pid: %d) releasing the dummy ringbuffer...\n", \
        current->comm, current->pid);
    return 0;
}

/**
 * @brief Get the number of bytes are available for reading in the upper and lower parts of the ringbuffer, respectively
 * @param[in] dev Ringbuffer device handle
 * @param[out] upper Bytes in the upper part of the buffer
 * @param[out] lower Bytes in the lower part of the buffer
 */
static inline void dummy_split_read_space(struct dummy_device *dev, size_t * upper, size_t * lower) {

    /* Remember that we start reading at dev->readidx + 1 (mod dev->ringsize) */
    size_t readidx = ACTUAL_READ_IDX(dev);

    if (dev->writeidx > readidx) {

        /* No index permutation, all data "ahead of" the read index */
        *upper = dev->writeidx - readidx;
        *lower = 0;

    } else {

        /* Indices permuted - calculate sizes of both chunks of data */
        *upper = dev->ringsize - readidx;
        *lower = dev->writeidx;
    }
}

/**
 * @brief Get the number of free space in the upper and lower parts of the ringbuffer, respectively
 * @param[in] dev Ringbuffer device handle
 * @param[out] upper Space in the upper part of the buffer
 * @param[out] lower Space in the lower part of the buffer
 */
static inline void dummy_split_write_space(struct dummy_device *dev, size_t * upper, size_t * lower) {

    if (dev->writeidx > dev->readidx) {

        /* No index permutation - calculate size of both chunks of data, i.e.
         * "ahead of" the write index and "behind" the read index */
        *upper = dev->ringsize - dev->writeidx;
        *lower = dev->readidx;

    } else {

        /* Indices permuted - free space only "between" the pointers */
        *upper = dev->readidx - dev->writeidx;
        *lower = 0;
    }
}

static ssize_t dummy_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {

    /* Recover the device structure */
    struct dummy_device * dev = filp->private_data;
    size_t upper;
    size_t lower;

    /* Enter critical section */
    if (down_interruptible(&dev->sem)) {

        return -ERESTARTSYS;
    }

    while (RINGBUFFER_EMPTY(dev)) {

        /* Release the lock before sleeping or returning */
        up(&dev->sem);
        if (filp->f_flags & O_NONBLOCK) {

            /* Requested no blocking */
            return -EAGAIN;
        }

        if (wait_event_interruptible(dev->readq, !RINGBUFFER_EMPTY(dev))) {

            /* Return -RESTARTSYS to the caller and let the virtual
             * filesystem layer handle the signal, i.e. either restart
             * the system call or return -EINTR to userspace. */
            return -ERESTARTSYS;
        }

        /* Reacquire the lock and recheck the condition */
        if (down_interruptible(&dev->sem)) {

            return -ERESTARTSYS;
        }
    }

    /* Calculate how much data can be read from the upper and bottom
     * part of the buffer, respectively */
    dummy_split_read_space(dev, &upper, &lower);

    upper = min(count, upper);
    lower = min(lower, count - upper);

    /* Read from the upper part of the buffer */
    if (upper && copy_to_user(buf, RINGBUFFER_READ_ADDR(dev), upper)) {

        up(&dev->sem);
        return -EFAULT;
    }

    if (lower && copy_to_user(buf + upper, &dev->buf[0], lower)) {

        up(&dev->sem);
        return -EFAULT;
    }

    /* Update the read index */
    RINGBUFFER_READ_IDX_UPDATE(dev, upper + lower);

    /* Exit critical section */
    up(&dev->sem);

    /* Wake up writers */
    wake_up_interruptible(&dev->writeq);

    return upper + lower;
}

static ssize_t dummy_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {

    /* Recover the device structure */
    struct dummy_device * dev = filp->private_data;
    size_t upper;
    size_t lower;

    /* Acquire the semaphore */
    if (down_interruptible(&dev->sem)) {

        /* Failed to acquire the lock */
        return -ERESTARTSYS;
    }

    while (RINGBUFFER_FULL(dev)) {

        /* Release the lock before sleeping or returning */
        up(&dev->sem);
        if (filp->f_flags & O_NONBLOCK) {

            /* Requested no blocking */
            return -EAGAIN;
        }

        if (wait_event_interruptible(dev->writeq, !RINGBUFFER_FULL(dev))) {

            /* Return -RESTARTSYS to the caller and let the virtual
             * filesystem layer handle the signal, i.e. either restart
             * the system call or return -EINTR to userspace. */
            return -ERESTARTSYS;
        }

        /* Reacquire the lock and recheck the condition */
        if (down_interruptible(&dev->sem)) {

            return -ERESTARTSYS;
        }
    }

    /* Calculate how much data can be written to the upper and bottom
     * part of the buffer, respectively */
    dummy_split_write_space(dev, &upper, &lower);

    upper = min(count, upper);
    lower = min(lower, count - upper);

    /* Write into the upper part of the buffer */
    if (upper && copy_from_user(RINGBUFFER_WRITE_ADDR(dev), buf, upper)) {

        up(&dev->sem);
        return -EFAULT;
    }

    /* Write into the lower part of the buffer if any data left */
    if (lower && copy_from_user(&dev->buf[0], buf + upper, lower)) {

        up(&dev->sem);
        return -EFAULT;
    }

    /* Increment the write index */
    RINGBUFFER_WRITE_IDX_UPDATE(dev, upper + lower);

    /* Exit critical section */
    up(&dev->sem);

    /* Awaken readers (all non-exclusive waiters - without WQ_FLAG_EXCLUSIVE)
     * and exactly one exclusive waiter) */
    wake_up_interruptible(&dev->readq);

    return upper + lower;
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A dummy device driver");
