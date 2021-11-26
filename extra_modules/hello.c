#include <linux/module.h>

void yee_kernel_test(void)
{
    printk("yee brgin\n");
    printk("yee end\n");
    
}
EXPORT_SYMBOL_GPL(yee_kernel_test);

static int __init hello_init(void)
{
	printk("hello init\n");
    yee_kernel_test();
	return 0;
}


static void __exit hello_exit(void)
{
	printk("hello exit\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("yeshen");
MODULE_DESCRIPTION("hello example");
MODULE_LICENSE("GPL");

