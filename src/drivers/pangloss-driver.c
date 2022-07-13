#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

static int __init pangloss_init(void) {

    printk(KERN_INFO "Hello best of all possible worlds\n");
    return 0;
}

static void __exit pangloss_exit(void) {

    printk(KERN_INFO "Goodbye, best of all possible worlds\n");
}

module_init(pangloss_init);
module_exit(pangloss_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A pangloss driver");
