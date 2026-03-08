#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include "logging.h"

static struct file_operations hyperfuzz_fops = {
    .owner = THIS_MODULE
};

static int major = -1;
static struct class* cls = NULL;

#define DEVICE_NAME "hyperfuzz"

static int __init hyperfuzz_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &hyperfuzz_fops);

    if (major < 0)
    {
        pr_err(LOG_PREFIX "Failed to register char device: error code: %d\n", major);
        return major;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif

    if (IS_ERR(cls))
    {
        pr_err(LOG_PREFIX "Failed to create class for device: error code: %ld\n", PTR_ERR(cls));
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(cls);
    }
    
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    
    pr_info(LOG_PREFIX "Created device on /dev/" DEVICE_NAME);
    return 0;
}

static void __exit hyperfuzz_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);

    pr_info(LOG_PREFIX "Module exited\n");
}

module_init(hyperfuzz_init);
module_exit(hyperfuzz_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey Polivoda");
MODULE_DESCRIPTION("Hypervisor that allows user programs to experiment with CPU");
