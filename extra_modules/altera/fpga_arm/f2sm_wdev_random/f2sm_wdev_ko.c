
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
//#include <linux/clk.h>
#include <linux/io.h>
 #include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/uio_driver.h>

#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/list.h>
//#include <asm/uaccess.h>
#include <linux/uaccess.h>

#define _IOC_F2SMW_START 0x0501
#define _IOC_F2SMW_STOP  0x0502
#define _IOC_F2SMW_PAUSE 0x0503
#define _IOC_F2SMW_DQBUF 0x0504

#define RAM_SPAN	(2*1024*1024)

struct f2sm_buf_q
{
	int mem[4];   // 2^2; (4-1)=0x3
	int head;
	int tail;
};

struct f2sm_read_info_t
{
	uint32_t irq_count;
	int mem_index;
};


/* misc device - f2smw_ram_dev */
struct f2smw_ram_dev {
	struct semaphore sem;
	unsigned long open_count;
	unsigned long release_count;
	unsigned long write_count;
	unsigned long write_byte_count;
    struct uio_info *p_uio_info;
	//unsigned char *cur_io_buf;
	int cur_io_buf_index;
	//unsigned char *io_buf;
};

static struct f2smw_ram_dev the_f2smw_ram_dev = {
	/*
	   .sem = initialize this at runtime before it is needed
	 */
	.open_count = 0,
	.release_count = 0,
	.write_count = 0,
	.write_byte_count = 0,
	.p_uio_info = NULL,
	//.cur_io_buf = NULL,
	.cur_io_buf_index = -1,
};

struct mem_ep
{
    char *mem_buf;
    struct list_head entry;
};

//static LIST_HEAD(g_mem_queue);
//static spinlock_t g_memque_lock;

static spinlock_t g_irq_lock;
static wait_queue_head_t g_irq_wait_queue;
static uint32_t g_irq_count;
static struct f2sm_buf_q g_avail_membuf_queue; 

static inline uint32_t get_current_irq_count(void)
{
	uint32_t current_count;
	unsigned long flags;

	spin_lock_irqsave(&g_irq_lock, flags);
	current_count = g_irq_count;
	spin_unlock_irqrestore(&g_irq_lock, flags);
	return current_count;
}

#define IS_F2SM_QUEUE_FULL(buf) (((buf->tail + 1) & 0x3) == buf->head) 

static int InitQueueF2sm(struct f2sm_buf_q *buf) 
{
	buf->tail = buf->head = 0;
	memset(buf->mem, 0, sizeof(buf->mem));
	return 0;
}

//MAXQSIZE = 4; 
static int EnQueueF2sm(struct f2sm_buf_q *buf,int mem_index)
{
	if (((buf->tail + 1) & 0x3) == buf->head)  // %4 equal to &0x3 
	{
		return -1;//full
	}
	buf->mem[buf->tail] = mem_index;
	buf->tail = (buf->tail + 1) & 0x3; //set // %4 equal to &0x3 
	return 0;
}

static int  DeQueueF2sm(struct f2sm_buf_q *buf) 
{
	//若队列不空，则删除Q的对头元素，用e返回其值
	int mem_index = -1;
	if (buf->tail == buf->head)
	{
		return -1;
	}
	mem_index = buf->mem[buf->head];
	buf->head = (buf->head + 1) &0x3;   // %4 equal to &0x3 
	return mem_index;
}

//read and wait for interrupt
//count -- input buffer size
static ssize_t
f2smw_ram_dev_read(struct file *fp, char __user *user_buffer,
		  size_t count, loff_t *offset)
{
	struct f2smw_ram_dev *dev = fp->private_data;
	uint32_t cur_irq_cnt;
	char *io_buf;
	size_t cur_count = count;
	size_t the_size = cur_count;
	int mem_index = -1;
	int readpos;
	cur_irq_cnt = get_current_irq_count();

	//pr_info("f2smw_map_dev_read enter\n");
	//pr_info("         fp = 0x%08X\n", (uint32_t)fp);
	//pr_info("user_buffer = 0x%08X\n", (uint32_t)user_buffer);
	//pr_info("      count = 0x%08X\n", (uint32_t)count);
	//pr_info("    *offset = 0x%08X\n", (uint32_t)*offset);

	if (down_interruptible(&dev->sem)) {
		pr_info("f2smw_map_dev_read sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	if (*offset != 0) {
		up(&dev->sem);
		pr_info("f2smw_map_dev_read offset != 0 exit\n");
		return -EINVAL;
	}

	mem_index = DeQueueF2sm(&g_avail_membuf_queue);

	if((mem_index != 0) &&(mem_index != 1))// wait for idle mem
	{
		//pr_info(" cur_irq_cnt = 0x%08X\n", (uint32_t)cur_irq_cnt); // perhaps irq happened just here, can't wait it
		while (cur_irq_cnt == get_current_irq_count()) {
			up(&dev->sem);
			if (wait_event_interruptible(g_irq_wait_queue,
							(cur_irq_cnt !=
							get_current_irq_count()))) {
				pr_info("f2smw_map_dev_read wait interrupted exit\n");
				return -ERESTARTSYS;
			}

			mem_index = DeQueueF2sm(&g_avail_membuf_queue);
			if((mem_index != 0) &&(mem_index != 1))
			{
				pr_info("f2smw_map_dev_read get interrupt but DeQueueF2sm error, exit\n");
				return -ERESTARTSYS;				
			}

			if (down_interruptible(&dev->sem)) {
				pr_info("f2smw_map_dev_read sem interrupted exit\n");
				return -ERESTARTSYS;
			}
		}
	}

	//perhaps get interrupt before wait it
	if((mem_index != 0) &&(mem_index != 1))
	{
		mem_index = DeQueueF2sm(&g_avail_membuf_queue);
		if((mem_index != 0) &&(mem_index != 1))
		{
			//wrong here
			up(&dev->sem);
			pr_info("f2smw_map_dev_read wait_event_interruptible dull exit\n");
			return -EFAULT;		
		}
	}

	readpos = cur_count/3;
	//readpos = 0;
	the_f2smw_ram_dev.cur_io_buf_index = mem_index;
	the_size = dev->p_uio_info->mem[mem_index].size;
	if(readpos >= the_size)
	{
		pr_info("f2smw_ram_dev_read pos too big error exit\n");
		return -ERESTARTSYS;		
	}

	if (count > (the_size - readpos))    //sizeof(struct f2sm_read_info_t)
	{
		cur_count = the_size - readpos;
	}

#if 1	
	io_buf = dev->p_uio_info->mem[mem_index].internal_addr;	
	if (copy_to_user(user_buffer, io_buf+readpos, cur_count)) {
		up(&dev->sem);
		pr_info("f2smw_map_dev_read copy_to_user exit\n");
		return -EFAULT;
	}
#else
	if (copy_to_user(user_buffer, &cur_irq_cnt, sizeof(uint32_t))) {
		up(&dev->sem);
		pr_info("f2smw_map_dev_read copy_to_user exit\n");
		return -EFAULT;
	}
#endif
	up(&dev->sem);
	//pr_info("f2smw_map_dev_read exit\n");
	return cur_count;
}


static int f2smw_ram_dev_open(struct inode *ip, struct file *fp)
{
	struct f2smw_ram_dev *dev = &the_f2smw_ram_dev;
	//pr_info("f2smw_ram_dev_open enter\n");
	//pr_info("ip = 0x%08X\n", (uint32_t)ip);
	//pr_info("fp = 0x%08X\n", (uint32_t)fp);

	if (down_interruptible(&dev->sem)) {
		pr_info("f2smw_ram_dev_open sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	fp->private_data = dev;
	dev->open_count++;

	InitQueueF2sm(&g_avail_membuf_queue);
	the_f2smw_ram_dev.cur_io_buf_index = -1;  //initially no resp data got

	up(&dev->sem);
	pr_info("f2smw_ram_dev_open exit\n");
	return 0;
}

static int f2smw_ram_dev_release(struct inode *ip, struct file *fp)
{
	struct f2smw_ram_dev *dev = fp->private_data;
	pr_info("f2smw_ram_dev_release enter\n");
	pr_info("ip = 0x%08X\n", (uint32_t)ip);
	pr_info("fp = 0x%08X\n", (uint32_t)fp);

	if (down_interruptible(&dev->sem)) {
		pr_info("f2smw_ram_dev_open sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	dev->release_count++;

	InitQueueF2sm(&g_avail_membuf_queue);
	the_f2smw_ram_dev.cur_io_buf_index = -1;

	up(&dev->sem);
	pr_info("f2smw_ram_dev_release exit\n");
	return 0;
}

#if 0
long f2smw_ram_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	/*
	 * work
	 */

	if (down_interruptible(&dev->sem)) {
		pr_info("f2smw_map_dev_read sem interrupted exit\n");
		return -1;
	}

	switch (cmd) {
	case _IOC_F2SMW_DQBUF:
	{
		struct mem_ep *ep;
		spin_lock_irq(&g_memque_lock);
		if (!list_empty(&g_mem_queue)) 
		{
			struct list_head *tmp;
			tmp = mem_queue.next;
			list_del(tmp);
			tmp->next = NULL;
			tmp->prev = NULL;
			spin_unlock_irq(&g_memque_lock);
			ep = list_entry(tmp, struct mem_ep, entry);
			the_f2smw_ram_dev.cur_io_buf = ep->mem_buf;
		}
		else
		{
			/* code */
			int ret;
			up(&dev->sem);
			spin_unlock_irq(&g_memque_lock);
			if (wait_event_interruptible(g_irq_wait_queue,(!list_empty(&g_mem_queue))) 
			{
				pr_info("f2smw_map_dev_read wait interrupted exit\n");
				return -1;
			}
			if (down_interruptible(&dev->sem)) 
			{
				pr_info("f2smw_map_dev_read sem interrupted exit\n");
				return -1;
			}

			//if (!list_empty(&g_mem_queue))  // should be true
			{
				spin_lock_irq(&g_memque_lock);
				struct list_head *tmp;
				tmp = mem_queue.next;
				list_del(tmp);
				tmp->next = NULL;
				tmp->prev = NULL;
				spin_unlock_irq(&g_memque_lock);
				ep = list_entry(tmp, struct mem_ep, entry);
				the_f2smw_ram_dev.cur_io_buf = ep->mem_buf;
			}
		}
		break;
	}
	case _IOC_F2SMW_START:
	case _IOC_F2SMW_STOP:
	case _IOC_F2SMW_PAUSE:
		break;

	default:
		break;
	}

	up(&dev->sem);
	return retval;
}

#endif

static const struct file_operations f2smw_ram_dev_fops = {
	.owner = THIS_MODULE,
	.open = f2smw_ram_dev_open,
	.release = f2smw_ram_dev_release,
	//.write = f2smw_ram_dev_write,
	.read = f2smw_ram_dev_read,
	//.unlocked_ioctl = f2smw_ram_dev_ioctl,
};

static struct miscdevice f2smw_ram_dev_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "fpgaw_ram",
	.fops = &f2smw_ram_dev_fops,
};

/* prototypes */
struct dev_data_st
{
    struct uio_info the_uio_info;
    //struct platform_driver *pDriver;
    int mem0_irq;
    int mem1_irq;
};

static struct of_device_id f2smw_driver_dt_ids[] = {
	{
	 .compatible = "f2sm_wdev,driver-1.0"},
	{ /* end of table */ }
};

static struct platform_driver the_platform_driver;

/* globals */
static struct semaphore g_dev_probe_sem;
static int g_platform_probe_flag=0;
static void *g_ioremap_addr0=NULL;
static void *g_ioremap_addr1=NULL;

irqreturn_t f2smw_driver_interrupt_handler(int irq, void *data)
{
	struct dev_data_st *priv = (struct dev_data_st *)data;
	int mem_index = -1;

	spin_lock(&g_irq_lock);
	if(irq == priv->mem0_irq)
	{
		// WARN_ON(!list_empty(&g_mem_queue));
#if 1
		mem_index = 0;
#else	//dyna queue 
		struct mem_ep *pNode = NULL;
		pNode = (struct mem_ep *)malloc(sizeof(struct mem_ep));
		memset(pNode, 0, sizeof(struct mem_ep));

		/* init node i */
		pNode->mem_buf = priv->the_uio_info.mem[0].internal_addr;         // pingpong should add 0 and 1
		spin_lock(&g_memque_lock);
		list_add_tail(&pNode->entry, &g_mem_queue);
		spin_unlock(&g_memque_lock);
#endif
	}
	else  // two irq  correspond to pingpong buffer
	{
		mem_index = 1;		
	}	

	the_f2smw_ram_dev.p_uio_info = &priv->the_uio_info;
	//the_f2smw_ram_dev.cur_io_buf = the_f2smw_ram_dev.p_uio_info->mem[0].internal_addr;
	if(EnQueueF2sm(&g_avail_membuf_queue,mem_index) < 0)
	{
		pr_info("irq %d recved but EnQueueF2sm wrong! \n",irq);;
	}
	/* increment the IRQ count */
	g_irq_count++;
	spin_unlock(&g_irq_lock);
	wake_up_interruptible(&g_irq_wait_queue);

	pr_info("irq %d recved! \n",irq);
	return IRQ_HANDLED;
}

static int platform_probe(struct platform_device *pdev)
{
	int ret_val;
	int irq;
    int i;

	struct resource res0,res1;
	struct device_node *np0 = NULL;
	struct device_node *np1 = NULL;
	struct dev_data_st *priv=NULL;
	//struct mem_ep *pNode = NULL;

	pr_info("platform_probe enter\n");

	ret_val = -EBUSY;

	/* acquire the probe lock */
	if (down_interruptible(&g_dev_probe_sem))
		return -ERESTARTSYS;

	if (g_platform_probe_flag != 0)
		goto bad_exit_return;

	ret_val = -EINVAL;

	InitQueueF2sm(&g_avail_membuf_queue);

	/* get our interrupt resource */
	irq = platform_get_irq(pdev, 0);            //  platform_get_irq(pdev, 1)  -> mem1_irq
	if (irq < 0) 
	{
		dev_err(&pdev->dev, "irq not available\n");
		goto bad_exit_return;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret_val = -ENOMEM;
		dev_err(&pdev->dev, "unable to kmalloc\n");
		goto bad_exit_release_dev_data;
	}
    //priv->pDriver = pdev;
	priv->mem0_irq = irq;
	pr_info("mem0_irq is %d\n",irq);

	/* Get reserved memory region from Device-tree */
	np0 = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np0) {
		dev_err(&pdev->dev, "No %s 0 specified\n", "memory-region");
		goto bad_exit_release_dev_data;
	}

	np1 = of_parse_phandle(pdev->dev.of_node, "memory-region", 1);
	if (!np1) {
		dev_err(&pdev->dev, "No %s 1 specified\n", "memory-region");
		goto bad_exit_release_dev_data;
	}

	ret_val = of_address_to_resource(np0, 0, &res0);
	if (ret_val) {
		dev_err(&pdev->dev, "No memory address assigned to the region\n");
		goto bad_exit_release_dev_data;
	}

	ret_val = of_address_to_resource(np1, 0, &res1);
	if (ret_val) {
		dev_err(&pdev->dev, "No memory address assigned to the region\n");
		goto bad_exit_release_dev_data;
	}

	priv->the_uio_info.mem[0].memtype = UIO_MEM_PHYS;
	priv->the_uio_info.mem[0].addr = res0.start;
	priv->the_uio_info.mem[0].size = resource_size(&res0);
	priv->the_uio_info.mem[0].name = "f2sm_uio_driver_hw_region0";
	priv->the_uio_info.mem[0].internal_addr = (void *)priv->the_uio_info.mem[0].addr;

	/* ioremap our memory region */
	g_ioremap_addr0 = memremap(priv->the_uio_info.mem[0].addr, priv->the_uio_info.mem[0].size,MEMREMAP_WT); 
	if (g_ioremap_addr0 == NULL) {
		pr_err("ioremap failed: g_f2smw_driver_base_addr\n");
		goto bad_exit_release_mem_region;
	}
	dev_info(&pdev->dev, "Allocated reserved memory, vaddr: 0x%p, paddr: 0x%08X\n", g_ioremap_addr0, priv->the_uio_info.mem[0].addr);
	priv->the_uio_info.mem[0].internal_addr = g_ioremap_addr0;

	priv->the_uio_info.mem[1].memtype = UIO_MEM_PHYS;
	priv->the_uio_info.mem[1].addr = res1.start;
	priv->the_uio_info.mem[1].size = resource_size(&res1);
	priv->the_uio_info.mem[1].name = "f2sm_uio_driver_hw_region1";
	priv->the_uio_info.mem[1].internal_addr = (void *)priv->the_uio_info.mem[1].addr;

	/* ioremap our memory region */
	g_ioremap_addr1 = memremap(priv->the_uio_info.mem[1].addr, priv->the_uio_info.mem[1].size, MEMREMAP_WT); 
	if (g_ioremap_addr1 == NULL) {
		pr_err("ioremap failed: g_f2smw_driver_base_addr\n");
		if(g_ioremap_addr0)
		{
			iounmap(g_ioremap_addr0);	
			g_ioremap_addr0 = NULL;
		}			
		goto bad_exit_iounmap;
	}
	dev_info(&pdev->dev, "Allocated reserved memory, vaddr: 0x%p, paddr: 0x%08X\n", g_ioremap_addr1, priv->the_uio_info.mem[1].addr);
	priv->the_uio_info.mem[1].internal_addr = g_ioremap_addr1;

	for (i = 2; i < MAX_UIO_MAPS; i++)
		priv->the_uio_info.mem[i].size = 0;

	platform_set_drvdata(pdev, priv);

	ret_val = -EBUSY;
	/* register our interrupt handler */
	init_waitqueue_head(&g_irq_wait_queue);
	spin_lock_init(&g_irq_lock);
	//spin_lock_init(&g_memque_lock);
	//INIT_LIST_HEAD(&g_mem_queue);

	g_irq_count = 0;

	/* register our interrupt handler */
	ret_val = request_irq(priv->mem0_irq,
			      f2smw_driver_interrupt_handler,
			      0,
			      "fpgaw-mem0-irq",
			      priv);

	if (ret_val) {
		pr_err("request_irq failed");
		goto bad_exit_iounmap;
	}

	ret_val = -EBUSY;

	/* register misc device dev_ram */
	sema_init(&the_f2smw_ram_dev.sem, 1);

	the_f2smw_ram_dev.p_uio_info = &priv->the_uio_info;
	//the_f2smw_ram_dev.cur_io_buf = priv->the_uio_info.mem[0].internal_addr;
	the_f2smw_ram_dev.cur_io_buf_index = -1;  // initially no resp data got

	ret_val = misc_register(&f2smw_ram_dev_device);
	if (ret_val != 0) {
		pr_warn("Could not register device \"f2smw_ram\"...");
		goto bad_exit_freeirq;
	}


	g_platform_probe_flag = 1;
	up(&g_dev_probe_sem);
	pr_info("platform_probe exit\n");
	return 0;

bad_exit_freeirq:
	free_irq(priv->mem0_irq, priv);
bad_exit_iounmap:
	if(g_ioremap_addr0)
    {
		iounmap(g_ioremap_addr0);
        g_ioremap_addr0 = NULL;
    }
	if(g_ioremap_addr1)
    {
		iounmap(g_ioremap_addr1);
        g_ioremap_addr1 = NULL;
    }
bad_exit_release_mem_region:
	if(priv->the_uio_info.mem[0].internal_addr)
	{
		priv->the_uio_info.mem[0].internal_addr = NULL;
	}
	if(priv->the_uio_info.mem[1].internal_addr)
	{
		priv->the_uio_info.mem[1].internal_addr = NULL;
	}	
bad_exit_release_dev_data:
	kfree(priv);
bad_exit_return:
	up(&g_dev_probe_sem);
	pr_info("platform_probe bad_exit\n");
	return ret_val;
}

static int platform_remove(struct platform_device *pdev)
{
	struct dev_data_st *priv = NULL;

	pr_info("platform_remove enter\n");

	misc_deregister(&f2smw_ram_dev_device);

	priv = platform_get_drvdata(pdev);
	free_irq(priv->mem0_irq, priv);
	if(g_ioremap_addr0)
    {
		iounmap(g_ioremap_addr0);
        g_ioremap_addr0 = NULL;
    }
	if(g_ioremap_addr1)
    {
		iounmap(g_ioremap_addr1);
        g_ioremap_addr1 = NULL;
    }

	//release_mem_region(g_f2smw_driver_base_addr, g_f2smw_driver_size);
	if(priv->the_uio_info.mem[0].internal_addr)
	{
		priv->the_uio_info.mem[0].internal_addr = NULL;
	}
	if(priv->the_uio_info.mem[1].internal_addr)
	{
		priv->the_uio_info.mem[1].internal_addr = NULL;
	}

	if (down_interruptible(&g_dev_probe_sem))
		return -ERESTARTSYS;

	g_platform_probe_flag = 0;
	up(&g_dev_probe_sem);

	pr_info("platform_remove exit\n");
	return 0;
}

MODULE_DEVICE_TABLE(of, f2smw_driver_dt_ids);

static struct platform_driver the_platform_driver = {
	.probe = platform_probe,
	.remove = platform_remove,
	.driver = {
		   .name = "f2sm_fpgaw_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = f2smw_driver_dt_ids,
		   },
	/*
	   .shutdown = unused,
	   .suspend = unused,
	   .resume = unused,
	   .id_table = unused,
	 */
};

static int f2smw_init(void)
{
	int ret_val;
	pr_info("f2smw_init enter\n");

	sema_init(&g_dev_probe_sem, 1);

	ret_val = platform_driver_register(&the_platform_driver);
	if (ret_val != 0) {
		pr_err("platform_driver_register returned %d\n", ret_val);
		return ret_val;
	}

	pr_info("f2smw_init exit\n");
	return 0;
}

static void f2smw_exit(void)
{
	pr_info("f2smw_exit enter\n");

	platform_driver_unregister(&the_platform_driver);

	pr_info("f2smw_exit exit\n");
}

module_init(f2smw_init);
module_exit(f2smw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Student One <dso@company.com>");
MODULE_DESCRIPTION("Demonstration Module 5 - reserve memory and enable IRQ");
//MODULE_VERSION("1.0");
