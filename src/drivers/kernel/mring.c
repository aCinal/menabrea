#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/random.h>
#include "include/mring.h"

static int order = 10;
module_param(order, int, S_IRUGO);
MODULE_PARM_DESC(order, "Base 2 logarithm of the rings sizes (in pages)");

static unsigned int burst = 50;
module_param(burst, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(burst, "Burst size when simulating pushes to the buffer");

static unsigned long period = 1000;
module_param(period, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(period, "Period (in milliseconds) of the timer used to simulate pushes to the buffer");

struct mring_kern {
    struct device *ctrl_device;
    struct device *buff_device;
    struct mring_ctrl_file *shared_ctrl;
    unsigned char *shared_buff;
    /* Safe copy of shared_ctrl */
    unsigned long safe_write_index;
    unsigned long safe_mask;
    dev_t ctrl_devno;
    dev_t buff_devno;

    /* Timer for simulating pushes to the buffer */
    struct timer_list timer;
};

/* The size of struct mring_ctrl_file in pages */
#define MRING_CTRL_PAGES  ( (sizeof(struct mring_ctrl_file) - 1) / PAGE_SIZE + 1 )

static struct mring_kern *mrings;
static struct class *class;

static void mring_timer_callback(struct timer_list *timer)
{
    struct mring_kern *mring = container_of(timer, struct mring_kern, timer);

    unsigned long burst_size = READ_ONCE(burst);
    /* Find available space */
    unsigned long read_index = READ_ONCE(mring->shared_ctrl->read_index);
    unsigned long write_index = mring->safe_write_index;
    unsigned long mask = mring->safe_mask;
    unsigned long available_space = (read_index - write_index - 1) & mask;

    if (burst_size <= available_space) {

        /* There is enough room, write random bytes */
        unsigned long upper_space = -write_index & mask;
        unsigned long lower_space = read_index;
        get_random_bytes(&mring->shared_buff[write_index], upper_space);
        get_random_bytes(mring->shared_buff, lower_space);

        mring->safe_write_index = (write_index + burst_size) & mask;
        /* Signal the change to userspace */
        mring->shared_ctrl->write_index = mring->safe_write_index;
        /* Renew the mask if destroyed by userspace */
        mring->shared_ctrl->mask = mask;
    }

    /* Reschedule the timer */
    unsigned long timer_period = READ_ONCE(period);
    mod_timer(timer, jiffies + msecs_to_jiffies(timer_period));
}

static int mring_create(struct mring_kern *mring, int ctrl_major, int buff_major, int minor)
{
    int ret;
    struct device *device;
    (void) memset(mring, 0, sizeof(*mring));

    /* Allocate a control block shared with userspace */
    mring->shared_ctrl = (void *) get_zeroed_page(GFP_USER);
    if (!mring->shared_ctrl)
        return -ENOMEM;
    /* Initialize the control block */
    mring->shared_ctrl->read_index = 0;
    mring->shared_ctrl->write_index = 0;
    mring->shared_ctrl->order = order;
    mring->shared_ctrl->mask = (PAGE_SIZE << order) - 1;
    mring->safe_write_index = mring->shared_ctrl->write_index;
    mring->safe_mask = mring->shared_ctrl->mask;

    /* Allocate a ringbuffer shared with userspace */
    mring->shared_buff = (void *) __get_free_pages(GFP_USER | __GFP_ZERO, order);
    if (!mring->shared_buff) {
        ret = -ENOMEM;
        goto error;
    }

    mring->ctrl_devno = MKDEV(ctrl_major, minor);
    mring->buff_devno = MKDEV(buff_major, minor);
    device = device_create(class, NULL, mring->buff_devno, mring, "mring%d_buff", minor);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        goto error;
    }
    mring->buff_device = device;

    device = device_create(class, NULL, mring->ctrl_devno, mring, "mring%d_ctrl", minor);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        goto error;
    }
    mring->ctrl_device = device;

    timer_setup(&mring->timer, mring_timer_callback, 0);

    return 0;

error:
    if (mring->ctrl_device)
        device_destroy(class, mring->ctrl_devno);
    if (mring->buff_device)
        device_destroy(class, mring->buff_devno);
    if (mring->shared_buff)
        free_pages((unsigned long) mring->shared_buff, order);
    if (mring->shared_ctrl)
        free_page((unsigned long) mring->shared_ctrl);
    return ret;
}

static void mring_destroy(struct mring_kern *mring)
{
    timer_shutdown_sync(&mring->timer);
    device_destroy(class, mring->ctrl_devno);
    device_destroy(class, mring->buff_devno);
    free_pages((unsigned long) mring->shared_buff, order);
    free_page((unsigned long) mring->shared_ctrl);
}

static int mring_ctrl_open(struct inode *inodp, struct file *filp)
{
    int minor = iminor(inodp);
    struct mring_kern *mring = &mrings[minor];
    filp->private_data = mring;

    /* TODO: Do some refcounting? */

    return 0;
}

static int mring_ctrl_release(struct inode *inodp, struct file *filp)
{
    /* TODO: Do some refcounting? */
    return 0;
}

static int mring_ctrl_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct mring_kern *mring = filp->private_data;

    unsigned long pfn = virt_to_phys(mring->shared_ctrl) >> PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;

    /* Ensure the entire control structure is being mapped */
    if (size != PAGE_SIZE * MRING_CTRL_PAGES)
        return -EINVAL;

    /* Ensure no offset */
    if (vma->vm_pgoff)
        return -EINVAL;

    int err = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (err)
        return err;

    return 0;
}

static int mring_buff_open(struct inode *inodp, struct file *filp)
{
    int minor = iminor(inodp);
    struct mring_kern *mring = &mrings[minor];
    filp->private_data = mring;

    /* TODO: Do some refcounting? */

    return 0;
}

static int mring_buff_release(struct inode *inodp, struct file *filp)
{
    /* TODO: Do some refcounting? */
    return 0;
}

static int mring_buff_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct mring_kern *mring = filp->private_data;

    unsigned long pfn = virt_to_phys(mring->shared_buff) >> PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;

    /* Ensure the entire ringbuffer is being mapped */
    if (size != PAGE_SIZE << order)
        return -EINVAL;

    /* Ensure no offset */
    if (vma->vm_pgoff)
        return -EINVAL;

    /* Do not allow writeable mappings */
    if (vma->vm_flags & VM_WRITE)
        return -EINVAL;

    int err = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (err)
        return err;

    return 0;
}

static struct file_operations mring_ctrl_fops = {
    .owner = THIS_MODULE,
    .open = mring_ctrl_open,
    .release = mring_ctrl_release,
    .mmap = mring_ctrl_mmap,
};

static struct file_operations mring_buff_fops = {
    .owner = THIS_MODULE,
    .open = mring_buff_open,
    .release = mring_buff_release,
    .mmap = mring_buff_mmap,
};

static int __init mring_init(void)
{
    /* Create one ring per core */
    unsigned int num_rings = nr_cpu_ids;
    int rings_created = 0;
    int buff_major = -1;
    int ctrl_major = -1;
    int ret;
    unsigned int timer_period = READ_ONCE(period);

    pr_info("Initializing %d mrings...\n", num_rings);
    /* Allocate a list of kernelside ring descriptors */
    mrings = kmalloc(num_rings * sizeof(struct mring_kern), GFP_KERNEL);
    if (!mrings)
        return -ENOMEM;

    /* Create a /sys/class entry */
    class = class_create("mring");
    if (IS_ERR(class)) {
        ret = PTR_ERR(class);
        goto error;
    }

    /* Register two character devices: one for buffers and one for control blocks */
    ret = __register_chrdev(0, 0, num_rings, "mring_buff", &mring_buff_fops);
    if (ret < 0)
        goto error;
    buff_major = ret;
    ret = __register_chrdev(0, 0, num_rings, "mring_ctrl", &mring_ctrl_fops);
    if (ret < 0)
        goto error;
    ctrl_major = ret;

    while (rings_created < num_rings) {
        ret = mring_create(&mrings[rings_created], ctrl_major, buff_major, rings_created);
        if (ret)
            goto error;
        rings_created++;
    }

    /* Arm the timers */
    while (rings_created--)
        mod_timer(&mrings[rings_created].timer, msecs_to_jiffies(timer_period));

    return 0;

error:
    while (rings_created > 0)
        mring_destroy(&mrings[--rings_created]);
    if (buff_major > 0)
        unregister_chrdev(buff_major, "mring_buff");
    if (ctrl_major > 0)
        unregister_chrdev(ctrl_major, "mring_ctrl");
    if (!IS_ERR(class))
        class_destroy(class);
    if (mrings)
        kfree(mrings);
    return ret;
}

static void __exit mring_exit(void)
{
    unsigned int num_rings = nr_cpu_ids;
    /* Recover the buffer and control devices major numbers from the first mring descriptor */
    BUG_ON(num_rings == 0);
    int buff_major = MAJOR(mrings[0].buff_devno);
    int ctrl_major = MAJOR(mrings[0].ctrl_devno);

    pr_info("Destroying %d mrings...\n", num_rings);

    /* Destroy the mrings */
    for (int i = 0; i < num_rings; i++)
        mring_destroy(&mrings[i]);

    /* Unregister character devices from the kernel */
    unregister_chrdev(buff_major, "mring_buff");
    unregister_chrdev(ctrl_major, "mring_ctrl");
    /* Destroy the /sys class */
    class_destroy(class);
    /* Free the array of mring descriptors */
    kfree(mrings);
}

module_init(mring_init);
module_exit(mring_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A PoC memory-mappable ringbuffer");
