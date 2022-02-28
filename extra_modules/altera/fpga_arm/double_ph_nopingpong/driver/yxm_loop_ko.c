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
#include <linux/uaccess.h>


#define _IOC_F2SM_START 0x0501
#define _IOC_F2SM_STOP  0x0502
#define _IOC_F2SM_PAUSE 0x0503
#define _IOC_F2SM_DQBUF 0x0504
#define RAM_SPAN	(8*1024*1024)
#define NON_STOP 0
#define NORMAL_STOP 1
#define ACCIDENT_STOP 2
#define DONE 1
#define UNDONE 0
#define PH1_USERBUF_OFFSET 	(16*1024*1024)
#define PH1_OFFSET 	0x01000000
#define IS_F2SM_QUEUE_FULL(buf) (((buf->tail + 1) & 0x3) == buf->head) 

/*  */
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

struct f2sm_ram_dev {
	struct semaphore sem;
	unsigned long open_count;
	unsigned long release_count;
	unsigned long write_count;
	unsigned long write_byte_count;
    struct uio_info *p_uio_info;
	int cur_io_buf_index;
};

struct mem_ep
{
    char *mem_buf;
    struct list_head entry;
};

struct dev_data_st
{
    struct uio_info the_uio_info;
    int mem0_irq;
    int mem1_irq;
};

static int normal_stop_irq;
static int accident_stop_irq;
static spinlock_t g_irq_lock;
static wait_queue_head_t g_irq_wait_queue_down;
static wait_queue_head_t g_irq_wait_queue_up;
static uint32_t g_irq_count_down;
static uint32_t g_irq_count_up;
static struct f2sm_buf_q g_avail_membuf_queue_down; 
static struct f2sm_buf_q g_avail_membuf_queue_up;
static struct semaphore g_dev_probe_sem;	//防止同时insmod的并发问题
static int g_platform_probe_flag=0;			//使得只能insmod一次，除非先rmmod
static void *g_ioremap_addr0=NULL;			//上行dma虚拟地址
static void *g_ioremap_addr1=NULL;			//下行dma虚拟地址
static uint32_t which_interrput_down = NON_STOP;
static uint32_t which_interrput_up = NON_STOP;
static uint32_t print_state_up = UNDONE;
static uint32_t print_state_down = UNDONE;


static struct platform_driver the_platform_driver;
static struct f2sm_ram_dev the_f2sm_ram_dev_down = {
	.open_count = 0,
	.release_count = 0,
	.write_count = 0,
	.write_byte_count = 0,
	.p_uio_info = NULL,
	.cur_io_buf_index = -1,
};
static struct f2sm_ram_dev the_f2sm_ram_dev_up = {
	.open_count = 0,
	.release_count = 0,
	.write_count = 0,
	.write_byte_count = 0,
	.p_uio_info = NULL,
	.cur_io_buf_index = -1,
};

static inline void do_nothing(void)
{
	return;
}

static inline int InitQueueF2sm(struct f2sm_buf_q *buf) 
{
	buf->tail = buf->head = 0;
	memset(buf->mem, 0, sizeof(buf->mem));
	return 0;
}

static inline int EnQueueF2sm(struct f2sm_buf_q *buf,int mem_index)
{
	if (((buf->tail + 1) & 0x3) == buf->head)  // %4 equal to &0x3 
	{
		return -1;//full
	}
	buf->mem[buf->tail] = mem_index;
	buf->tail = (buf->tail + 1) & 0x3; //set // %4 equal to &0x3 
	return 0;
}

static inline int DeQueueF2sm(struct f2sm_buf_q *buf) 
{
	//若队列不空，则删除Q的对头元素，用e返回其值
	int mem_index = -1;
	if (buf->tail == buf->head)
	{
		return -1;
	}
	mem_index = buf->mem[buf->head];
	buf->head = (buf->head + 1) & 0x3;   // %4 equal to &0x3 
	return mem_index;
}

static inline uint32_t get_current_irq_count_down(void)
{
	uint32_t current_count;
	unsigned long flags;

	spin_lock_irqsave(&g_irq_lock, flags);
	current_count = g_irq_count_down;
	spin_unlock_irqrestore(&g_irq_lock, flags);
	
	return current_count;
}

static inline uint32_t get_current_irq_count_up(void)
{
	uint32_t current_count;
	unsigned long flags;

	spin_lock_irqsave(&g_irq_lock, flags);
	current_count = g_irq_count_up;
	spin_unlock_irqrestore(&g_irq_lock, flags);
	
	return current_count;
}

static irqreturn_t f2sm_driver_interrupt_handler_down(int irq, void *data)
{
	int mem_index = -1;

	spin_lock(&g_irq_lock);

	mem_index = 0;
	
	if (EnQueueF2sm(&g_avail_membuf_queue_down, mem_index) < 0)
		pr_info("down irq %d recved but EnQueueF2sm wrong! \n", irq);
	
	g_irq_count_down++;
	print_state_down = UNDONE;
	
	spin_unlock(&g_irq_lock);
	
	wake_up_interruptible(&g_irq_wait_queue_down);

	return IRQ_HANDLED;
}

static irqreturn_t f2sm_driver_interrupt_handler_up(int irq, void *data)
{
	int mem_index = -1;

	spin_lock(&g_irq_lock);
	
	mem_index = 0;	
	
	if (EnQueueF2sm(&g_avail_membuf_queue_up, mem_index) < 0)
		pr_info("up irq %d recved but EnQueueF2sm wrong! \n", irq);	
	
	g_irq_count_up++;
	print_state_up = UNDONE;
	
	spin_unlock(&g_irq_lock);	
	
	wake_up_interruptible(&g_irq_wait_queue_up);
	
	return IRQ_HANDLED;
}

static irqreturn_t normal_stop_interrupt_handler(int irq, void *data)
{
	pr_info("come the normal interrupt\n");

	spin_lock(&g_irq_lock);

	which_interrput_down = NORMAL_STOP;
	which_interrput_up = NORMAL_STOP;
	print_state_up = DONE;
	print_state_down = DONE;
	
	spin_unlock(&g_irq_lock);
	
	/* 唤醒睡眠进程 */
	wake_up_interruptible(&g_irq_wait_queue_down);
	wake_up_interruptible(&g_irq_wait_queue_up);
	
	return IRQ_HANDLED;
}

static irqreturn_t accident_stop_interrupt_handler(int irq, void *data)
{
	pr_info("come the accident interrupt\n");
	
	spin_lock(&g_irq_lock);

	which_interrput_down = ACCIDENT_STOP;
	which_interrput_up = ACCIDENT_STOP;
	print_state_up = DONE;
	print_state_down = DONE;
	
	spin_unlock(&g_irq_lock);
	
	/* 唤醒睡眠进程 */
	wake_up_interruptible(&g_irq_wait_queue_down);
	wake_up_interruptible(&g_irq_wait_queue_up);

	return IRQ_HANDLED;
}

/* 下行write */
static ssize_t
down_dev_write(struct file *fp,
		   const char __user *user_buffer, size_t count,
		   loff_t *offset)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_down;
	loff_t max_offset = RAM_SPAN;
	size_t temp_count = count;
	char *io_buf;
	int buf_index;
	int writepos;

	if (down_interruptible(&dev->sem)) {
		pr_info("down_dev_write sem interrupted exit\n");
		return -ERESTARTSYS;
	}	

	buf_index = dev->cur_io_buf_index;
	if (buf_index != 0) {
		pr_info("down_dev_write buf_index error exit\n");
		return -ERESTARTSYS;		
	}

	/* 下行dma映射后地址 */
	io_buf = dev->p_uio_info->mem[1].internal_addr;
	max_offset = dev->p_uio_info->mem[1].size;

	writepos = 0;
	if ((temp_count + writepos) > max_offset) {
		pr_info("down_dev_write count too big error exit\n");
		return -ERESTARTSYS;
	}

	dev->write_count++;

	if (temp_count > 0) {
		if (copy_from_user
		    (io_buf + writepos, user_buffer, temp_count)) {
			up(&dev->sem);
			pr_info("down_dev_write copy_from_user exit\n");
			return -EFAULT;
		}

        if (copy_from_user
		    (io_buf + PH1_OFFSET + writepos, user_buffer + PH1_USERBUF_OFFSET, temp_count)) {
			up(&dev->sem);
			pr_info("down_dev_write copy_from_user exit\n");
			return -EFAULT;
		}
	}

	dev->write_byte_count += temp_count;

	up(&dev->sem);
	
	return count;
}


/* 上行read
 * 收到fpga到hps的dma中断后,将hps侧的dma buf中的数据拷贝到应用层user_buffer中,
 * 拷贝的数据量大小为count字节,offset永远为初始化的值0
 */
static ssize_t
up_dev_read(struct file *fp, char __user *user_buffer,
		  size_t count, loff_t *offset)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_up;
	uint32_t cur_irq_cnt;
	char *io_buf;
	size_t cur_count = count;	
	size_t the_size = cur_count;
	int mem_index = -1;
	int readpos;
	unsigned long flags;

	if (down_interruptible(&dev->sem)) {
		pr_info("up_dev_read sem interrupted exit\n");
		return -ERESTARTSYS;
	}	

	cur_irq_cnt = get_current_irq_count_up();
	
	if (*offset != 0) {
		up(&dev->sem);
		pr_info("up_dev_read offset != 0 exit\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&g_irq_lock, flags);
	mem_index = DeQueueF2sm(&g_avail_membuf_queue_up);
	spin_unlock_irqrestore(&g_irq_lock, flags);	
	
	if (mem_index != 0) {  //wait for idle mem
		//这里有漏洞，虽然上行read没发生过，可能中断来的规律比较早或者比较晚
		//同样将cur_irq_cnt == get_current_irq_count_up()改为mem_index != 0
		while (mem_index != 0 && which_interrput_up == NON_STOP) {
			up(&dev->sem);	
			
			if (wait_event_interruptible(g_irq_wait_queue_up,
							(cur_irq_cnt != get_current_irq_count_up()) || 
							(which_interrput_up != NON_STOP) 
							)) {
				pr_info("up_dev_read wait interrupted exit\n");
				return -ERESTARTSYS;
			}

			/* 几种情况：
             * 1.单独dma中断 2.dma后跟打印中断 3.单独打印中断                 4.打印中断后跟dma中断
			 */
			spin_lock_irqsave(&g_irq_lock, flags);			
			mem_index = DeQueueF2sm(&g_avail_membuf_queue_up);
			spin_unlock_irqrestore(&g_irq_lock, flags);

			if (down_interruptible(&dev->sem)) {
				pr_info("up_dev_read sem interrupted exit\n");
				return -ERESTARTSYS;
			}
		}
	}

situation_1:
	spin_lock_irqsave(&g_irq_lock, flags);
	if (mem_index == 0 && which_interrput_up == NON_STOP && print_state_up == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		goto ret_to_user_with_dma_interrupt;
	}

situation_2:
	if (mem_index == 0 && which_interrput_up != NON_STOP && print_state_up == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("up_dev_read situation 2\n");
		goto ret_to_user_with_print_interrupt_after_dma_interrupt;
	}

situation_3:
	if (mem_index != 0 && which_interrput_up != NON_STOP && print_state_up == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("up_dev_read situation 3\n");
		goto ret_to_user_with_print_interrupt;
	}

situation_4:
	if (mem_index == 0 && which_interrput_up != NON_STOP && print_state_up == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("up_dev_read situation 4\n");
		goto ret_to_user_with_dma_interrupt_after_print_interrupt;	
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);


	/* 不符合以上4种情况的是错误情况 */
	pr_info("up_dev_read not one of four situation\n");
	pr_info("up_dev_read mem_index %d\n", mem_index);
	pr_info("up_dev_read which_interrput_up %d\n", which_interrput_up);
	pr_info("up_dev_read print_state_up %d\n", print_state_up);	
	up(&dev->sem);
	return -ERESTARTSYS;
	

ret_to_user_with_dma_interrupt:
ret_to_user_with_print_interrupt_after_dma_interrupt:
	readpos = 0;
	the_f2sm_ram_dev_up.cur_io_buf_index = mem_index;
	the_size = dev->p_uio_info->mem[mem_index].size;	//0x800000
	if (count > (the_size - readpos)) {   
		cur_count = the_size - readpos;		//我的设计中不可能
		up(&dev->sem);
		pr_info("up_dev_read pos too big exit\n");
		return -ERESTARTSYS;
	}
	io_buf = dev->p_uio_info->mem[mem_index].internal_addr;	
	if (copy_to_user(user_buffer, io_buf, cur_count)) {
		up(&dev->sem);
		pr_info("up_dev_read copy_to_user exit\n");
		return -EFAULT;
	}
	if (copy_to_user(user_buffer + PH1_USERBUF_OFFSET, io_buf + PH1_OFFSET, cur_count)) {
		up(&dev->sem);
		pr_info("up_dev_read copy_to_user exit\n");
		return -EFAULT;
	}

	/* 如果是情况2则接则往下走，情况1则返回用户,其他情况错误 */
	spin_lock_irqsave(&g_irq_lock, flags);
	if (likely(which_interrput_up == NON_STOP && print_state_up == UNDONE)) {	//情况1
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return cur_count;
	} else if (which_interrput_up != NON_STOP && print_state_up == DONE) {		//情况2
		do_nothing();
	} else {		//其他情况，错误
		pr_info("up_dev_read not situation 1 or 2, bad situation\n");
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return -ERESTARTSYS;
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);
	
ret_to_user_with_print_interrupt:
ret_to_user_with_dma_interrupt_after_print_interrupt:
	spin_lock_irqsave(&g_irq_lock, flags);
	if (which_interrput_up == NORMAL_STOP) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return 1;
	} else if (which_interrput_up == ACCIDENT_STOP) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return 2;
	} else {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return -ERESTARTSYS;
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);

}		  


/* 下行read 
 * 
 */
static ssize_t
down_dev_read(struct file *fp, char __user *user_buffer,
		size_t count, loff_t *offset)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_down;
	struct f2sm_read_info_t cur_read_info;
	uint32_t cur_irq_cnt;
	int mem_index;
	unsigned long flags;

	if (down_interruptible(&dev->sem)) {
	  pr_info("down_dev_read sem interrupted exit\n");
	  return -ERESTARTSYS;
	}	
	
	cur_irq_cnt = get_current_irq_count_down();

	if (*offset != 0) {
	  up(&dev->sem);
	  pr_info("down_dev_read offset != 0 exit\n");
	  return -EINVAL;
	}
	
	if (count != 8) {  //sizeof(struct f2sm_read_info_t)
		up(&dev->sem);
		pr_info("down_dev_read count != 8 exit\n");
		return -EINVAL;
	}	

	spin_lock_irqsave(&g_irq_lock, flags);
	mem_index = DeQueueF2sm(&g_avail_membuf_queue_down);
	spin_unlock_irqrestore(&g_irq_lock, flags);
	
	if (mem_index != 0) {  //wait for idle mem		
		//fuck!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//因为在上面的mem_index = DeQueueF2sm(&g_avail_membuf_queue_down);之前没中断，
		//然后mem_index = -1，进入while之前来了中断，此时mem_index还是-1,但是cur_irq_cnt已经不等于
		//get_current_irq_count_down了，所以直接跳过while，所以就出现了不是以下4种情况的问题
		//暂时解决方法：将cur_irq_cnt == get_current_irq_count_down()换成mem_index != 0或者直接去掉
		while (mem_index != 0 && which_interrput_down == NON_STOP) {
			up(&dev->sem);	
			
			if (wait_event_interruptible(g_irq_wait_queue_down,
							(cur_irq_cnt != get_current_irq_count_down()) || 
							(which_interrput_down != NON_STOP) 
							)) {
				pr_info("down_dev_read wait interrupted exit\n");
				return -ERESTARTSYS;
			}
	
			/* 几种情况：
             * 1.单独dma中断 2.dma后跟打印中断 3.单独打印中断                 4.打印中断后跟dma中断
			 */
			spin_lock_irqsave(&g_irq_lock, flags);
			mem_index = DeQueueF2sm(&g_avail_membuf_queue_down);
			spin_unlock_irqrestore(&g_irq_lock, flags);

			if (down_interruptible(&dev->sem)) {
				pr_info("down_dev_read sem interrupted exit\n");
				return -ERESTARTSYS;
			}
		}
	}

situation_1:
	spin_lock_irqsave(&g_irq_lock, flags);
	if (mem_index == 0 && which_interrput_down == NON_STOP && print_state_down == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		goto ret_to_user_with_dma_interrupt;
	}

situation_2:
	if (mem_index == 0 && which_interrput_down != NON_STOP && print_state_down == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("down_dev_read situation 2\n");
		goto ret_to_user_with_print_interrupt_after_dma_interrupt;
	}

situation_3:
	if (mem_index != 0 && which_interrput_down != NON_STOP && print_state_down == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("down_dev_read situation 3\n");
		goto ret_to_user_with_print_interrupt;
	}

situation_4:
	if (mem_index == 0 && which_interrput_down != NON_STOP && print_state_down == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("down_dev_read situation 4\n");
		goto ret_to_user_with_dma_interrupt_after_print_interrupt;	
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);


	/* 不符合以上4种情况的是错误情况 */
	pr_info("down_dev_read not one of four situation\n");
	pr_info("down_dev_read mem_index %d\n", mem_index);
	pr_info("down_dev_read which_interrput_down %d\n", which_interrput_down);
	pr_info("down_dev_read print_state_down %d\n", print_state_down); 
	up(&dev->sem);
	return -ERESTARTSYS;
	

ret_to_user_with_dma_interrupt:
ret_to_user_with_print_interrupt_after_dma_interrupt:
	cur_irq_cnt = get_current_irq_count_down();
	the_f2sm_ram_dev_down.cur_io_buf_index = mem_index;
	cur_read_info.irq_count = cur_irq_cnt;
	cur_read_info.mem_index = mem_index;

	if (copy_to_user(user_buffer, &cur_read_info, count)) {
		up(&dev->sem);
		pr_info("down_dev_read copy_to_user exit\n");
		return -EFAULT;
	}

	/* 如果是情况2则接则往下走，情况1则返回用户,其他情况错误 */
	spin_lock_irqsave(&g_irq_lock, flags);
	if (likely(which_interrput_down == NON_STOP && print_state_down == UNDONE)) {	//情况1
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return count;		//
	} else if (which_interrput_down != NON_STOP && print_state_down == DONE) {		//情况2
		do_nothing();
	} else {		//其他情况，错误
		pr_info("down_dev_read not situation 1 or 2, bad situation\n");
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return -ERESTARTSYS;
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);
	
ret_to_user_with_print_interrupt:
ret_to_user_with_dma_interrupt_after_print_interrupt:
	spin_lock_irqsave(&g_irq_lock, flags);
	if (which_interrput_down == NORMAL_STOP) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return 1;
	} else if (which_interrput_down == ACCIDENT_STOP) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return 2;
	} else {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return -ERESTARTSYS;
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);

}


/* 初始化上行驱动数据 */
static int up_dev_open(struct inode *ip, struct file *fp)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_up;

	if (down_interruptible(&dev->sem)) {
		pr_info("up_dev_open sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	fp->private_data = dev;		//设置为设备文件/dev/up_dev的私有数据
	dev->open_count++;

	/* 初始化没有可用的，因此要先transfer来个中断才会read到一个 */
	InitQueueF2sm(&g_avail_membuf_queue_up);
	the_f2sm_ram_dev_up.cur_io_buf_index = -1;

	up(&dev->sem);
	
	return 0;
}


/* 初始化下行驱动数据 */
static int down_dev_open(struct inode *ip, struct file *fp)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_down;

	if (down_interruptible(&dev->sem)) {
		pr_info("down_dev_open sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	fp->private_data = dev;		//设置为设备文件/dev/down_dev的私有数据
	dev->open_count++;

	/* 初始化就有一个，因此一开始直接读就一定会有一个 */
	InitQueueF2sm(&g_avail_membuf_queue_down);
	EnQueueF2sm(&g_avail_membuf_queue_down, 0);	
	the_f2sm_ram_dev_down.cur_io_buf_index = 0;

	up(&dev->sem);
	
	return 0;
}


/* 释放上行驱动数据 */
static int up_dev_release(struct inode *ip, struct file *fp)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_up;

	if (down_interruptible(&dev->sem)) {
		pr_info("up_dev_release sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	dev->release_count++;

	InitQueueF2sm(&g_avail_membuf_queue_up); 
	the_f2sm_ram_dev_up.cur_io_buf_index = -1;

	up(&dev->sem);
	
	return 0;
}

static int down_dev_release(struct inode *ip, struct file *fp)
{
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_down;

	if (down_interruptible(&dev->sem)) {
		pr_info("down_dev_release sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	dev->release_count++;

	InitQueueF2sm(&g_avail_membuf_queue_down); 
	the_f2sm_ram_dev_down.cur_io_buf_index = -1;

	up(&dev->sem);
	
	return 0;
}


static const struct file_operations up_dev_fops = {
	.owner = THIS_MODULE,
	.open = up_dev_open,
	.release = up_dev_release,
	.read = up_dev_read,
};

static struct miscdevice up_dev_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "up_dev",
	.fops = &up_dev_fops,
};

static const struct file_operations down_dev_fops = {
	.owner = THIS_MODULE,
	.open = down_dev_open,
	.release = down_dev_release,
	.write = down_dev_write,
	.read = down_dev_read,
	//.unlocked_ioctl = down_dev_ioctl,
};

static struct miscdevice down_dev_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "down_dev",
	.fops = &down_dev_fops,
};


static int platform_probe(struct platform_device *pdev)
{
    int i;
	int irq;
	int ret_val;

	struct resource res0,res1;
	struct device_node *np0 = NULL;
	struct device_node *np1 = NULL;
	struct dev_data_st *priv=NULL;

	pr_info("platform_probe enter\n");

	ret_val = -EBUSY;

	/* acquire the probe lock */
	if (down_interruptible(&g_dev_probe_sem))
		return -ERESTARTSYS;

	if (g_platform_probe_flag != 0)
		goto bad_exit_return;

	ret_val = -EINVAL;

	/* 平台驱动的私有数据 */
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret_val = -ENOMEM;
		dev_err(&pdev->dev, "unable to kmalloc\n");
		goto bad_exit_release_dev_data;
	}	

	/* 获取下行dma中断的虚拟中断号 */
	irq = platform_get_irq(pdev, 0);            
	if (irq < 0) 
	{
		dev_err(&pdev->dev, "irq0 not available\n");
		goto bad_exit_release_dev_data;
	}
	priv->mem0_irq = irq;

	/* 获取上行dma中断的虚拟中断号 */
	irq = platform_get_irq(pdev, 1); 
	if (irq < 0) 
	{
		dev_err(&pdev->dev, "irq1 not available\n");
		goto bad_exit_release_dev_data;
	}
	priv->mem1_irq = irq;	
	
	/* 获取结束打印中断的虚拟中断号 */
	normal_stop_irq = platform_get_irq(pdev, 2);            
	if (normal_stop_irq < 0) 
	{
		dev_err(&pdev->dev, "irq4 not available\n");
		goto bad_exit_release_dev_data;
	}
	accident_stop_irq = platform_get_irq(pdev, 3);            
	if (accident_stop_irq < 0) 
	{
		dev_err(&pdev->dev, "irq5 not available\n");
		goto bad_exit_release_dev_data;
	}	
 
	pr_info("down dma irq is %d\n", priv->mem0_irq);
	pr_info("up dma irq is %d\n", priv->mem1_irq);
	pr_info("normal stop irq is %d\n", normal_stop_irq);
	pr_info("accident stop irq is %d\n", accident_stop_irq);

	/* Get reserved memory region from Device-tree */
	np0 = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np0) {
		dev_err(&pdev->dev, "No %s 0 specified\n", "memory-region");
		goto bad_exit_release_dev_data;
	}

	np1 = of_parse_phandle(pdev->dev.of_node, "memory-region", 2);
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

	/* 上行 */
	priv->the_uio_info.mem[0].memtype = UIO_MEM_PHYS;
	priv->the_uio_info.mem[0].addr = res0.start;
	priv->the_uio_info.mem[0].size = resource_size(&res0);	
	priv->the_uio_info.mem[0].name = "f2sm_uio_driver_hw_region0";
	priv->the_uio_info.mem[0].internal_addr = (void *)priv->the_uio_info.mem[0].addr;
	g_ioremap_addr0 = memremap(priv->the_uio_info.mem[0].addr, 2*priv->the_uio_info.mem[0].size, MEMREMAP_WT); 
	if (g_ioremap_addr0 == NULL) {
		pr_err("ioremap failed: g_f2sm_driver_base_addr\n");
		goto bad_exit_release_mem_region;
	}
	dev_info(&pdev->dev, "Allocated reserved memory, vaddr: 0x%p, paddr: 0x%08X\n", g_ioremap_addr0, priv->the_uio_info.mem[0].addr);
	priv->the_uio_info.mem[0].internal_addr = g_ioremap_addr0;

	/* 下行 */
	priv->the_uio_info.mem[1].memtype = UIO_MEM_PHYS;
	priv->the_uio_info.mem[1].addr = res1.start;
	priv->the_uio_info.mem[1].size = resource_size(&res1);
	priv->the_uio_info.mem[1].name = "f2sm_uio_driver_hw_region1";
	priv->the_uio_info.mem[1].internal_addr = (void *)priv->the_uio_info.mem[1].addr;
	g_ioremap_addr1 = memremap(priv->the_uio_info.mem[1].addr, 2*priv->the_uio_info.mem[1].size, MEMREMAP_WT); 
	if (g_ioremap_addr1 == NULL) {
		pr_err("ioremap failed: g_f2sm_driver_base_addr\n");
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
	init_waitqueue_head(&g_irq_wait_queue_down);
	init_waitqueue_head(&g_irq_wait_queue_up);
	spin_lock_init(&g_irq_lock);

	g_irq_count_down = 0;
	g_irq_count_up = 0;

	/* 下行，arm发，fpga收 */
	ret_val = request_irq(priv->mem0_irq,
			      f2sm_driver_interrupt_handler_down,
			      0,
			      "fpga-read-irq",
			      priv);
	if (ret_val) {
		pr_err("request irq0 failed");
		goto bad_exit_iounmap;
	}

	/* 上行，arm收，fpga发 */
	ret_val = request_irq(priv->mem1_irq,
				  f2sm_driver_interrupt_handler_up,
				  0,
				  "fpga-write-irq",
				  priv);
	if (ret_val) {
		pr_err("request irq1 failed");
		goto bad_exit_freeirq0;
	}

	/* 注册正常结束打印中断和异常结束打印中断处理函数 */
	ret_val = request_irq(normal_stop_irq,
			      normal_stop_interrupt_handler,
			      0,
			      "yxm-irq4",
			      NULL);
	if (ret_val) {
		pr_err("request irq4 failed");
		goto bad_exit_freeirq0_1;
	}	

	ret_val = request_irq(accident_stop_irq,
			      accident_stop_interrupt_handler,
			      0,
			      "yxm-irq5",
			      NULL);
	if (ret_val) {
		pr_err("request irq5 failed");
		goto bad_exit_freeirq0_1_4;
	}	

	ret_val = -EBUSY;

	/* register misc device dev_ram */
	sema_init(&the_f2sm_ram_dev_down.sem, 1);
	sema_init(&the_f2sm_ram_dev_up.sem, 1);

	/* 文件私有数据和平台设备私有数据关联起来 */
	the_f2sm_ram_dev_down.p_uio_info = &priv->the_uio_info;
	the_f2sm_ram_dev_up.p_uio_info = &priv->the_uio_info;

	/* 注册杂项设备 */
	ret_val = misc_register(&up_dev_device);
	if (ret_val != 0) {
		pr_warn("Could not register device \"up_dev\"...");
		goto bad_exit_freeirq0_1_4_5;
	}
	ret_val = misc_register(&down_dev_device);
	if (ret_val != 0) {
		pr_warn("Could not register device \"down_dev\"...");
		goto bad_exit_unregister_up_dev_and_freeirq0_1_4_5;
	}
	
	g_platform_probe_flag = 1;
	up(&g_dev_probe_sem);
	pr_info("platform_probe exit\n");
	return 0;

bad_exit_unregister_up_dev_and_freeirq0_1_4_5:	
	misc_deregister(&up_dev_device);
bad_exit_freeirq0_1_4_5:
	free_irq(accident_stop_irq, priv);
bad_exit_freeirq0_1_4:
	free_irq(normal_stop_irq, priv);
bad_exit_freeirq0_1:
	free_irq(priv->mem1_irq, priv);
bad_exit_freeirq0:
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

	misc_deregister(&up_dev_device);
	misc_deregister(&down_dev_device);

	priv = platform_get_drvdata(pdev);
	free_irq(priv->mem0_irq, priv);
	free_irq(priv->mem1_irq, priv);
	free_irq(normal_stop_irq, NULL);
	free_irq(accident_stop_irq, NULL);
	
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

	//release_mem_region(g_f2sm_driver_base_addr, g_f2sm_driver_size);
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

static struct of_device_id f2sm_driver_dt_ids[] = {
	{ .compatible = "yxm_dev,driver-1.0"},
	{ /* end of table */ }
};


MODULE_DEVICE_TABLE(of, f2sm_driver_dt_ids);

static struct platform_driver the_platform_driver = {
	.probe = platform_probe,
	.remove = platform_remove,
	.driver = {
		   .name = "yxm_dev_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = f2sm_driver_dt_ids,
		   },

};

static int f2sm_init(void)
{
	int ret_val;
	pr_info("yxm_dev_init enter\n");

	sema_init(&g_dev_probe_sem, 1);

	ret_val = platform_driver_register(&the_platform_driver);
	if (ret_val != 0) {
		pr_err("platform_driver_register returned %d\n", ret_val);
		return ret_val;
	}

	pr_info("yxm_dev_init exit\n");
	return 0;
}

static void f2sm_exit(void)
{
	pr_info("yxm_dev_exit enter\n");

	platform_driver_unregister(&the_platform_driver);

	pr_info("yxm_dev_exit exit\n");
}

module_init(f2sm_init);
module_exit(f2sm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Student One <dso@company.com>");
MODULE_DESCRIPTION("Demonstration Module 5 - reserve memory and enable IRQ");

