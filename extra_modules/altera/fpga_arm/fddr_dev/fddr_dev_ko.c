
/*
 * Copyright (C) 2015 Altera Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>

#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

/* prototypes */
static struct platform_driver the_platform_driver;

/* globals */
static struct semaphore g_fddr_dev_probe_sem;
static int g_fddr_platform_probe_flag;
static int g_fddr_driver_irq0;  // write ready
static int g_fddr_driver_irq1;  // read ready

static struct semaphore g_fddr_io_dev_sem;
static spinlock_t g_irq_lock;
static wait_queue_head_t g_irq_wait_queue;
static uint32_t g_irq_count;

static inline uint32_t get_current_irq_count(void)
{
	uint32_t current_count;
	unsigned long flags;

	spin_lock_irqsave(&g_irq_lock, flags);
	current_count = g_irq_count;
	spin_unlock_irqrestore(&g_irq_lock, flags);
	return current_count;
}


//read and wait for interrupt
static ssize_t
fddr_io_dev_read(struct file *fp, char __user *user_buffer,
		  size_t count, loff_t *offset)
{
	uint32_t cur_irq_cnt;
	cur_irq_cnt = get_current_irq_count();

	//pr_info("fddr_io_dev_read enter\n");
	//pr_info("         fp = 0x%08X\n", (uint32_t)fp);
	//pr_info("user_buffer = 0x%08X\n", (uint32_t)user_buffer);
	//pr_info("      count = 0x%08X\n", (uint32_t)count);
	//pr_info("    *offset = 0x%08X\n", (uint32_t)*offset);

	if (down_interruptible(&g_fddr_io_dev_sem)) {
		pr_info("fddr_io_dev_read sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	if (*offset != 0) {
		up(&g_fddr_io_dev_sem);
		pr_info("fddr_io_dev_read offset != 0 exit\n");
		return -EINVAL;
	}

	if (count != 4) 
	{
		up(&g_fddr_io_dev_sem);
		pr_info("fddr_io_dev_read count != 4 exit\n");
		return -EINVAL;
	}

	//pr_info(" cur_irq_cnt = 0x%08X\n", (uint32_t)cur_irq_cnt);
	while (cur_irq_cnt == get_current_irq_count()) {
		up(&g_fddr_io_dev_sem);
		if (wait_event_interruptible(g_irq_wait_queue,
						(cur_irq_cnt !=
						get_current_irq_count()))) {
			pr_info("fddr_io_dev_read wait interrupted exit\n");
			return -ERESTARTSYS;
		}

		if (down_interruptible(&g_fddr_io_dev_sem)) {
			pr_info("fddr_io_dev_read sem interrupted exit\n");
			return -ERESTARTSYS;
		}
	}

	cur_irq_cnt = get_current_irq_count();

	if (copy_to_user(user_buffer, &cur_irq_cnt, count)) {
		up(&g_fddr_io_dev_sem);
		pr_info("fddr_io_dev_read copy_to_user exit\n");
		return -EFAULT;
	}

	up(&g_fddr_io_dev_sem);
	//pr_info("fddr_io_dev_read exit\n");
	return count;
}


static int fddr_io_dev_open(struct inode *ip, struct file *fp)
{
	//pr_info("fddr_io_dev_open enter\n");
	//pr_info("ip = 0x%08X\n", (uint32_t)ip);
	//pr_info("fp = 0x%08X\n", (uint32_t)fp);

	if (down_interruptible(&g_fddr_io_dev_sem)) {
		pr_info("fddr_io_dev_open sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	up(&g_fddr_io_dev_sem);
	//pr_info("fddr_io_dev_open exit\n");
	return 0;
}

static int fddr_io_dev_release(struct inode *ip, struct file *fp)
{
	//pr_info("fddr_io_dev_release enter\n");
	//pr_info("ip = 0x%08X\n", (uint32_t)ip);
	//pr_info("fp = 0x%08X\n", (uint32_t)fp);
	if (down_interruptible(&g_fddr_io_dev_sem)) {
		pr_info("fddr_io_dev_open sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	up(&g_fddr_io_dev_sem);
	//pr_info("fddr_io_dev_release exit\n");
	return 0;
}


irqreturn_t fddr_driver_interrupt_handler(int irq, void *dev_id)
{
	spin_lock(&g_irq_lock);
	if(irq == g_fddr_driver_irq0)
	{
		;  // read ready
	}
	else  // two irq  correspond to pingpong buffer
	{
		;	//write ready	
	}	

	/* increment the IRQ count */
	g_irq_count++;
	spin_unlock(&g_irq_lock);
	wake_up_interruptible(&g_irq_wait_queue);

	pr_info("irq %d recved! \n",irq);
	return IRQ_HANDLED;
}


static const struct file_operations fddr_io_dev_fops = {
	.owner = THIS_MODULE,
	.open = fddr_io_dev_open,
	.release = fddr_io_dev_release,
	.read = fddr_io_dev_read,
};

static struct miscdevice fddr_io_dev_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "fddr_io_dev",
	.fops = &fddr_io_dev_fops,
};



static int platform_probe(struct platform_device *pdev)
{
	int ret_val;
	int irq0,irq1;

	//pr_info("platform_probe enter\n");
	ret_val = -EBUSY;

	/* acquire the probe lock */
	if (down_interruptible(&g_fddr_dev_probe_sem))
		return -ERESTARTSYS;

	if (g_fddr_platform_probe_flag != 0)
		goto bad_exit_return;

	ret_val = -EINVAL;

	/* get our interrupt resource */
	irq0 = platform_get_irq(pdev, 0);
	if (irq0 < 0) {
		pr_err("irq0 not available\n");
		goto bad_exit_return;
	}

	g_fddr_driver_irq0 = irq0;

	/* get our interrupt resource */
	irq1 = platform_get_irq(pdev, 1);
	if (irq1 < 0) {
		pr_err("irq1 not available\n");
		goto bad_exit_return;
	}
	g_fddr_driver_irq1 = irq1;

	ret_val = -EBUSY;

	/* register our interrupt handler */
	ret_val = request_irq(g_fddr_driver_irq0,
			      fddr_driver_interrupt_handler,
			      0,
			      the_platform_driver.driver.name,
			      &the_platform_driver);

	if (ret_val) {
		pr_err("request_irq0 failed");
		goto bad_exit_return;
	}

	ret_val = -EBUSY;

	/* register our interrupt handler */
	init_waitqueue_head(&g_irq_wait_queue);
	spin_lock_init(&g_irq_lock);
	g_irq_count = 0;

	/* register our interrupt handler */
	ret_val = request_irq(g_fddr_driver_irq1,
			      fddr_driver_interrupt_handler,
			      0,
			      the_platform_driver.driver.name,
			      &the_platform_driver);

	if (ret_val) {
		pr_err("request_irq1 failed");
		goto bad_exit_freeirq0;
	}

	ret_val = -EBUSY;
	/* register misc device dev_ram */
	sema_init(&g_fddr_io_dev_sem, 1);
	ret_val = misc_register(&fddr_io_dev_device);
	if (ret_val != 0) {
		pr_warn("Could not register device \"demo_ram\"...");
		goto bad_exit_freeirq1;
	}


	g_fddr_platform_probe_flag = 1;
	up(&g_fddr_dev_probe_sem);
	//pr_info("platform_probe exit\n");
	return 0;

bad_exit_freeirq1:
	free_irq(g_fddr_driver_irq1, &the_platform_driver);
bad_exit_freeirq0:
	free_irq(g_fddr_driver_irq0, &the_platform_driver);
bad_exit_return:
	up(&g_fddr_dev_probe_sem);
	//pr_info("platform_probe bad_exit\n");
	return ret_val;
}

static int platform_remove(struct platform_device *pdev)
{
	//pr_info("platform_remove enter\n");
	misc_deregister(&fddr_io_dev_device);

	free_irq(g_fddr_driver_irq0, &the_platform_driver);
	free_irq(g_fddr_driver_irq1, &the_platform_driver);

	if (down_interruptible(&g_fddr_dev_probe_sem))
		return -ERESTARTSYS;

	g_fddr_platform_probe_flag = 0;
	up(&g_fddr_dev_probe_sem);

	//pr_info("platform_remove exit\n");
	return 0;
}

static struct of_device_id fddr_driver_dt_ids[] = {
	{
	 .compatible = "fddr_dev,driver-1.0"},
	{ /* end of table */ }
};

MODULE_DEVICE_TABLE(of, fddr_driver_dt_ids);

static struct platform_driver the_platform_driver = {
	.probe = platform_probe,
	.remove = platform_remove,
	.driver = {
		   .name = "fddr_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = fddr_driver_dt_ids,
		   },
	/*
	   .shutdown = unused,
	   .suspend = unused,
	   .resume = unused,
	   .id_table = unused,
	 */
};

static int fddr_init(void)
{
	int ret_val;
	pr_info("fddr_init enter\n");

	sema_init(&g_fddr_dev_probe_sem, 1);

	ret_val = platform_driver_register(&the_platform_driver);
	if (ret_val != 0) {
		pr_err("platform_driver_register returned %d\n", ret_val);
		return ret_val;
	}

	pr_info("fddr_init exit\n");
	return 0;
}

static void fddr_exit(void)
{
	pr_info("fddr_exit enter\n");

	platform_driver_unregister(&the_platform_driver);

	pr_info("fddr_exit exit\n");
}

module_init(fddr_init);
module_exit(fddr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Student One <dso@company.com>");
MODULE_DESCRIPTION("Demonstration Module 5 - reserve memory and enable IRQ");
//MODULE_VERSION("1.0");
