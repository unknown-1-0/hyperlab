#include <linux/module.h>

static int __init hyperfuzz_init(void)
{
    return 0;
}

static void __exit hyperfuzz_exit(void)
{

}

module_init(hyperfuzz_init);
module_exit(hyperfuzz_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey Polivoda");
MODULE_DESCRIPTION("Hypervisor that allows user programs to experiment with CPU");
