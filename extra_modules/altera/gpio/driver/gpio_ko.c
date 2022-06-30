/*
 * Copyright (C) 2022 Hanglory Ye Zheng
 * This driver is for gpio operation
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <asm-generic/ioctl.h>
#include <linux/gpio/consumer.h>
#include <linux/semaphore.h>
#include <linux/slab.h>


#define GPIO_DIR_IN 0
#define GPIO_DIR_OUT 1
#define GPIO_DIR_NONE -1

#define GPIO_VAL_LOW 0
#define GPIO_VAL_HIGH 1
#define GPIO_VAL_NONE -1


#define MAX_GPIO_HPS_NUM (29 + 29 + 27)
#define MAX_GPIO_EXPAND_NUM 32
#define GPIO_EXPAND_OFFSET MAX_GPIO_HPS_NUM



/* 和用户协定的gpio描述格式 */
typedef struct han_gpio_desc {
	char *name;		
	int ind;		
	int dir;		
	int val;
} han_gpio_desc_t;

typedef struct han_gpio_priv_data {
	struct platform_device *gpios_pdev;
	struct device *gpios_dev;
	struct device_node *gpios_node;	
	size_t gpio_num;
	size_t hps_gpio_num;
	size_t expand_gpio_num;
	struct semaphore sem;
	unsigned long count;
} han_gpio_priv_data_t;

static struct semaphore g_dev_probe_sem;	//防止同时insmod的并发问题
static int g_platform_probe_flag = 0;		//使得只能insmod一次，除非先rmmod
static struct gpio_desc *g_gpios[MAX_GPIO_HPS_NUM + MAX_GPIO_EXPAND_NUM];
static han_gpio_priv_data_t g_gpio_priv_data;

/* 检查gpio_dev_write时用户传入的han_gpio_desc方向和值是否有效
 * 传入方向为输入时,传入任何值都不设置,但为了规范传入GPIO_VAL_NONE,传入其他给警告信息
 * 传入方向为输出时,一定要传入有效值,只能是GPIO_VAL_LOW或者GPIO_VAL_HIGH,否则错误返回
 * 传入方向为NONE时,一定要传入有效值,只能是GPIO_VAL_LOW或者GPIO_VAL_HIGH,否则错误返回
 * 传入方向为其他值直接错误返回
 */
static int check_han_gpio_descs(han_gpio_desc_t *descs, size_t count)
{
	int i;
	
	for (i = 0; i < count; i++) {
		if (descs[i].dir == GPIO_DIR_IN) {
			if (descs[i].val != GPIO_VAL_NONE)
				pr_warn("warn: check_han_gpio_descs shoulde pass in GPIO_VAL_NONE\n");
		} else if ((descs[i].dir == GPIO_DIR_OUT) || 
					(descs[i].dir == GPIO_DIR_NONE)) {
			if ((descs[i].val != GPIO_VAL_LOW) && 
				(descs[i].val != GPIO_VAL_HIGH)) {
				pr_err("error: check_han_gpio_descs must pass in GPIO_VAL_LOW or GPIO_VAL_HIGH\n");
				return -ERESTARTSYS;
			}
		} else {
			pr_err("check_han_gpio_descs dir error\n");
			return -ERESTARTSYS;
		}
	}
	
	return 0;
}


static int gpio_dev_open(struct inode *ip, struct file *fp)
{
	struct han_gpio_priv_data *priv = &g_gpio_priv_data;
	
	if (down_interruptible(&priv->sem)) {
		pr_err("gpio_dev_open sem interrupted exit\n");
		return -EINTR;
	}

	fp->private_data = priv;		

	priv->count++;	
	up(&priv->sem);
	
	return 0;
}


int gpio_dev_flush(struct file *fp, fl_owner_t id)
{
	struct han_gpio_priv_data *priv = fp->private_data;

	if (down_interruptible(&priv->sem)) {
		pr_err("gpio_dev_flush sem interrupted exit\n");
		return -EINTR;
	}	

	priv->count--;	
	up(&priv->sem);
	return 0;
}


static int gpio_dev_release(struct inode *ip, struct file *fp)
{
	return 0;
}

/* 设置一个或多个gpio的值和方向,传入的值和方向约束参考check_han_gpio_descs
 * user_buffer : 指向第一个han_gpio_desc_t,按数组方式排放
 * count : gpio即han_gpio_desc_t的个数
 * offset : 保留不使用
 */
static ssize_t gpio_dev_write(struct file *fp, const char __user *user_first_desc_addr, 
									size_t count, loff_t *offset)
{
	int ret;
	int gpio_i;
	int gpio_d;
	int gpio_v;
	size_t i;
	size_t gpio_num;
	han_gpio_desc_t *pgpio;
	han_gpio_desc_t *pgpios;
	struct gpio_desc *gpio_desc;
	
	struct han_gpio_priv_data *priv = fp->private_data;
	if (down_interruptible(&priv->sem)) {
		pr_err("gpio_dev_write sem interrupted exit\n");
		return -EINTR;
	}	

	gpio_num = count / sizeof(han_gpio_desc_t);
	if (gpio_num > priv->gpio_num) {
		pr_err("gpio_dev_write count more than gpio num\n");
		up(&priv->sem);
		return -EINVAL;
	}

	pgpios = (han_gpio_desc_t *)kzalloc(count, GFP_KERNEL);
	if (!pgpios) {
		pr_err("gpio_dev_write kzalloc failed\n");
		up(&priv->sem);
		return -ENOMEM;
	}

	ret = copy_from_user(pgpios, user_first_desc_addr, count);
	if (ret) {
		pr_err("gpio_dev_write Copy from user failed!\n");
		kfree(pgpios);
		up(&priv->sem);
		return -ENOMEM;
	}

	ret = check_han_gpio_descs(pgpios, gpio_num);
	if (ret) {
		pr_err("gpio_dev_write check_han_gpio_descs failed!\n");
		kfree(pgpios);
		up(&priv->sem);
		return -ERESTARTSYS;
	}

	for (i = 0, pgpio = pgpios; i < gpio_num; i++, pgpio++) {
		gpio_i = pgpio->ind;
		gpio_d = pgpio->dir;
		gpio_v = pgpio->val;
		gpio_desc = g_gpios[gpio_i];
		
		if (gpio_d == GPIO_DIR_IN)
			gpiod_direction_input(gpio_desc);
		else if (gpio_d == GPIO_DIR_OUT)
			gpiod_direction_output(gpio_desc, gpio_v);
		else
			gpiod_set_value_cansleep(gpio_desc, gpio_v);
	}

	kfree(pgpios);
	up(&priv->sem);

	return (ssize_t)count;
}

/* 获取一个或多个gpio的值,对输入或输出的gpio都可以,只能获取值
 * 最终返回给用户时都会覆盖掉原来的val,其他保持不变
 */
static ssize_t gpio_dev_read(struct file *fp, char __user *user_first_desc_addr,
									size_t count, loff_t *offset)
{
	int ret;
	int gpio_i;
	int gpio_v;
	size_t i;
	size_t gpio_num;
	han_gpio_desc_t *pgpio;
	han_gpio_desc_t *pgpios;
	struct gpio_desc *gpio_desc;
	
	struct han_gpio_priv_data *priv = fp->private_data;
	if (down_interruptible(&priv->sem)) {
		pr_err("gpio_dev_read sem interrupted exit\n");
		return -EINTR;
	}

	gpio_num = count / sizeof(han_gpio_desc_t);
	if (gpio_num > priv->gpio_num) {
		up(&priv->sem);
		pr_err("gpio_dev_read count more than gpio num\n");
		return -EINVAL;
	}

	pgpios = (han_gpio_desc_t *)kzalloc(count, GFP_KERNEL);
	if (!pgpios) {
		up(&priv->sem);
		pr_err("gpio_dev_read kzalloc failed\n");
		return -ENOMEM;
	}

	ret = copy_from_user(pgpios, user_first_desc_addr, count);
	if (ret) {
		pr_err("gpio_dev_read Copy from user failed\n");
		kfree(pgpios);
		up(&priv->sem);
		return -ENOMEM;
	}

	for (i = 0, pgpio = pgpios; i < gpio_num; i++, pgpio++) {
		gpio_i = pgpios[i].ind;
		gpio_desc = g_gpios[gpio_i];
		gpio_v = gpiod_get_value_cansleep(gpio_desc);
		pgpio->val = gpio_v;
	}

	ret = copy_to_user(user_first_desc_addr, pgpios, count);
	if (ret) {
		pr_err("gpio_dev_read Copy to user failed\n");
		kfree(pgpios);
		up(&priv->sem);
		return -ENOMEM;
	}	

	kfree(pgpios);
	up(&priv->sem);

	return (ssize_t)count;
}



static const struct file_operations gpio_dev_fops = {
	.owner = THIS_MODULE,
	.open = gpio_dev_open,
	.flush = gpio_dev_flush,
	.release = gpio_dev_release,
	.write = gpio_dev_write,
	.read = gpio_dev_read,
};

static struct miscdevice gpio_dev_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gpios",	//设备节点/dev/gpios
	.fops = &gpio_dev_fops,
};


static int gpio_platform_probe(struct platform_device *pdev)
{
	int ret, i;
	size_t hps_ngpio;
	size_t expand_ngpio;

	if (down_interruptible(&g_dev_probe_sem)) {
		pr_err("gpio_platform_probe sem interrupted exit\n");
		return -ERESTARTSYS;
	}	
	if (g_platform_probe_flag != 0) {
		pr_err("insmod repeat!\n");
		up(&g_dev_probe_sem);
		return -ERESTARTSYS;	
	}
	
	ret = of_property_read_u32(pdev->dev.of_node, "hps-ngpio", &hps_ngpio);
	if (ret || (hps_ngpio > MAX_GPIO_HPS_NUM)) {
		pr_err("get hps gpio num failed or hps gpio out of range!\n");
		up(&g_dev_probe_sem);
		return -ERESTARTSYS;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "expand-ngpio", &expand_ngpio);
	if (ret || (expand_ngpio > MAX_GPIO_EXPAND_NUM)) {
		pr_err("get expand gpio num failed or expand gpio out of range!\n");
		up(&g_dev_probe_sem);
		return -ERESTARTSYS;
	}	

	g_gpio_priv_data.gpios_pdev = pdev;
	g_gpio_priv_data.gpios_dev = &pdev->dev;
	g_gpio_priv_data.gpios_node = pdev->dev.of_node;
	g_gpio_priv_data.hps_gpio_num = hps_ngpio;
	g_gpio_priv_data.expand_gpio_num = expand_ngpio;
	g_gpio_priv_data.gpio_num = hps_ngpio + expand_ngpio;
	sema_init(&g_gpio_priv_data.sem, 1);
	g_gpio_priv_data.count = 0;

	for (i = 0; i < hps_ngpio; i++) 
		g_gpios[i] = devm_gpiod_get_index(&pdev->dev, "hps", i, GPIOD_ASIS);

	for (i = 0; i < expand_ngpio; i++)
		g_gpios[GPIO_EXPAND_OFFSET + i] = devm_gpiod_get_index(&pdev->dev, "expand", i, GPIOD_ASIS);
	
	
	/* 注册杂项设备 */
	ret = misc_register(&gpio_dev_device);
	if (ret) {
		pr_err("Could not register gpio misc device\n");
		up(&g_dev_probe_sem);
		return -ERESTARTSYS;
	}		

	g_platform_probe_flag = 1;
	up(&g_dev_probe_sem);
	return 0;
}

static int gpio_platform_remove(struct platform_device *pdev)
{
	int i;
	
	if (down_interruptible(&g_dev_probe_sem))
		return -ERESTARTSYS;

	misc_deregister(&gpio_dev_device);		

	for (i = 0; i < g_gpio_priv_data.expand_gpio_num; i++) {
		devm_gpiod_put(&pdev->dev, g_gpios[GPIO_EXPAND_OFFSET + i]);
		g_gpios[GPIO_EXPAND_OFFSET + i] = NULL;
	}	
	
	for (i = 0; i < g_gpio_priv_data.hps_gpio_num; i++) {
		devm_gpiod_put(&pdev->dev, g_gpios[i]);
		g_gpios[i] = NULL;
	}	
	
	g_platform_probe_flag = 0;
	
	up(&g_dev_probe_sem);	
	return 0;
}


static struct of_device_id gpio_driver_dt_ids[] = {
	{ .compatible = "gpio_dev,driver-2.0"},
	{ /* end of table */ }
};


MODULE_DEVICE_TABLE(of, gpio_driver_dt_ids);

static struct platform_driver gpio_platform_driver = {
	.probe = gpio_platform_probe,
	.remove = gpio_platform_remove,
	.driver = {
		.name = "gpio_dev_driver",
		.owner = THIS_MODULE,
		.of_match_table = gpio_driver_dt_ids,
	},
};

static int gpio_init(void)
{
	int ret_val;
	sema_init(&g_dev_probe_sem, 1);
	ret_val = platform_driver_register(&gpio_platform_driver);
	if (ret_val != 0) {
		pr_err("gpio platform driver register failed\n");
		return ret_val;
	}
	pr_info("gpio init\n");
	return 0;
}

static void gpio_exit(void)
{
	platform_driver_unregister(&gpio_platform_driver);
	sema_init(&g_dev_probe_sem, 0);
	pr_info("gpio exit\n");
}

module_init(gpio_init);
module_exit(gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeshen");
MODULE_DESCRIPTION("Hanglory 3.0 gpio operation driver-2.0");


