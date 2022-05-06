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
#include <asm-generic/ioctl.h>
#include <linux/reset.h>


#include "internel.h"


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
static void __iomem *g_ioremap_addr_up=NULL;			//上行dma buffer虚拟地址
static void __iomem *g_ioremap_addr_down=NULL;			//下行dma buffer虚拟地址
static void __iomem *g_ioremap_addr_f2h=NULL;			//f2h桥虚拟地址
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
	.pdev = NULL,
};
static struct f2sm_ram_dev the_f2sm_ram_dev_up = {
	.open_count = 0,
	.release_count = 0,
	.write_count = 0,
	.write_byte_count = 0,
	.p_uio_info = NULL,
	.cur_io_buf_index = -1,
	.pdev = NULL,
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
	if (((buf->tail + 1) & 0x3) == buf->head)  //full
		return -1;
	
	buf->mem[buf->tail] = mem_index;
	buf->tail = (buf->tail + 1) & 0x3; //set // %4 equal to &0x3 
	return 0;
}

static inline int DeQueueF2sm(struct f2sm_buf_q *buf) 
{
	//若队列不空，则删除Q的对头元素，用e返回其值
	//若为空，直接返回负一
	int mem_index = -1;
	if (buf->tail == buf->head)		//empty
		return -1;
	
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
	int mem_index = DOWN_QUEUE_NOT_AVAILABLE;

	spin_lock(&g_irq_lock);

	mem_index = DOWN_QUEUE_AVAILABLE;
	
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
	int mem_index = UP_QUEUE_NOT_AVAILABLE;

	spin_lock(&g_irq_lock);
	
	mem_index = UP_QUEUE_AVAILABLE;	
	
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
	size_t temp_count = count;
	loff_t max_offset;
	char *io_buf;
	int buf_index;
	int writepos;

	if (down_interruptible(&dev->sem)) {
		pr_err("down_dev_write sem interrupted exit\n");
		return -ERESTARTSYS;
	}	

	buf_index = dev->cur_io_buf_index;
	if (buf_index != 0) {
		pr_err("down_dev_write buf_index error exit\n");
		return -ERESTARTSYS;		
	}

	/* 下行dma映射后地址 */
	io_buf = dev->p_uio_info->mem[DOWN_MEM_INDEX].internal_addr;
	max_offset = dev->p_uio_info->mem[DOWN_MEM_INDEX].size;

	writepos = 0;
	if ((temp_count + writepos) > max_offset) {
		pr_err("down_dev_write count too big error exit\n");
		return -ERESTARTSYS;
	}

	dev->write_count++;

	if (temp_count > 0) {
		if (copy_from_user
		    (io_buf + writepos, user_buffer, temp_count)) {
			up(&dev->sem);
			pr_err("down_dev_write copy_from_user exit\n");
			return -EFAULT;
		}

        if (copy_from_user
		    (io_buf + PH1_OFFSET + writepos, user_buffer + PH1_USERBUF_OFFSET, temp_count)) {
			up(&dev->sem);
			pr_err("down_dev_write copy_from_user exit\n");
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
	int mem_index = UP_QUEUE_NOT_AVAILABLE;
	int readpos;
	unsigned long flags;


	if (down_interruptible(&dev->sem)) {
		pr_err("up_dev_read sem interrupted exit\n");
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
	
	
	if (mem_index != UP_QUEUE_AVAILABLE) {  //wait for idle mem
		//这里有漏洞，虽然上行read没发生过，可能中断来的规律比较早或者比较晚
		//同样将cur_irq_cnt == get_current_irq_count_up()改为mem_index != 0
		while (mem_index != UP_QUEUE_AVAILABLE && which_interrput_up == NON_STOP) {
			up(&dev->sem);	
			
			if (wait_event_interruptible(g_irq_wait_queue_up,
							(cur_irq_cnt != get_current_irq_count_up()) || 
							(which_interrput_up != NON_STOP) 
							)) {
				pr_err("up_dev_read wait interrupted exit\n");
				return -ERESTARTSYS;
			}

			/* 几种情况：
             * 1.单独dma中断 2.dma后跟打印中断 3.单独打印中断                 4.打印中断后跟dma中断
			 */
			spin_lock_irqsave(&g_irq_lock, flags);			
			mem_index = DeQueueF2sm(&g_avail_membuf_queue_up);
			spin_unlock_irqrestore(&g_irq_lock, flags);

			if (down_interruptible(&dev->sem)) {
				pr_err("up_dev_read sem interrupted exit\n");
				return -ERESTARTSYS;
			}
		}
	}

	/* situation 1 */
	spin_lock_irqsave(&g_irq_lock, flags);
	if (mem_index == UP_QUEUE_AVAILABLE && which_interrput_up == NON_STOP && print_state_up == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		goto ret_to_user_with_dma_interrupt;
	}

	/* situation 2 */
	if (mem_index == UP_QUEUE_AVAILABLE && which_interrput_up != NON_STOP && print_state_up == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("up_dev_read situation 2\n");
		goto ret_to_user_with_print_interrupt_after_dma_interrupt;
	}

	/* situation 3 */
	if (mem_index != UP_QUEUE_AVAILABLE && which_interrput_up != NON_STOP && print_state_up == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("up_dev_read situation 3\n");
		goto ret_to_user_with_print_interrupt;
	}

	/* situation 4 */
	if (mem_index == UP_QUEUE_AVAILABLE && which_interrput_up != NON_STOP && print_state_up == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("up_dev_read situation 4\n");
		goto ret_to_user_with_dma_interrupt_after_print_interrupt;	
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);


	/* 不符合以上4种情况的是错误情况 */
	pr_err("up_dev_read not one of four situation\n");
	pr_err("up_dev_read mem_index %d\n", mem_index);
	pr_err("up_dev_read which_interrput_up %d\n", which_interrput_up);
	pr_err("up_dev_read print_state_up %d\n", print_state_up);	
	up(&dev->sem);
	return -ERESTARTSYS;
	

ret_to_user_with_dma_interrupt:
ret_to_user_with_print_interrupt_after_dma_interrupt:
	readpos = 0;
	the_f2sm_ram_dev_up.cur_io_buf_index = mem_index;
	the_size = dev->p_uio_info->mem[UP_MEM_INDEX].size;		
	if (count > (the_size - readpos)) {   
		cur_count = the_size - readpos;		//我的设计中不可能
		up(&dev->sem);
		pr_err("up_dev_read pos too big exit\n");
		return -ERESTARTSYS;
	}
	io_buf = dev->p_uio_info->mem[UP_MEM_INDEX].internal_addr;		
	
	if (copy_to_user(user_buffer, io_buf, cur_count)) {
		up(&dev->sem);
		pr_err("up_dev_read copy_to_user ph0 exit\n");
		return -EFAULT;
	}
	
	if (copy_to_user(user_buffer + PH1_USERBUF_OFFSET, io_buf + PH1_OFFSET, cur_count)) {
		up(&dev->sem);
		pr_err("up_dev_read copy_to_user ph1 exit\n");
		return -EFAULT;
	}

	/* 如果是情况2则接则往下走,情况1则返回用户,其他情况错误 */
	spin_lock_irqsave(&g_irq_lock, flags);
	if (likely(which_interrput_up == NON_STOP && print_state_up == UNDONE)) {	//情况1
		spin_unlock_irqrestore(&g_irq_lock, flags);
		up(&dev->sem);
		return cur_count;
	} else if (which_interrput_up != NON_STOP && print_state_up == DONE) {		//情况2
		do_nothing();
	} else {		//其他情况，错误
		pr_err("up_dev_read not situation 1 or 2, bad situation\n");
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
	  pr_err("down_dev_read sem interrupted exit\n");
	  return -ERESTARTSYS;
	}	
	
	cur_irq_cnt = get_current_irq_count_down();

	if (*offset != 0) {
	  up(&dev->sem);
	  pr_err("down_dev_read offset != 0 exit\n");
	  return -EINVAL;
	}
	
	if (count != 8) {  //sizeof(struct f2sm_read_info_t)
		up(&dev->sem);
		pr_err("down_dev_read count != 8 exit\n");
		return -EINVAL;
	}	

	spin_lock_irqsave(&g_irq_lock, flags);
	mem_index = DeQueueF2sm(&g_avail_membuf_queue_down);
	spin_unlock_irqrestore(&g_irq_lock, flags);
	
	if (mem_index != DOWN_QUEUE_AVAILABLE) {  //wait for idle mem		
		//fuck!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//因为在上面的mem_index = DeQueueF2sm(&g_avail_membuf_queue_down);之前没中断，
		//然后mem_index = -1，进入while之前来了中断，此时mem_index还是-1,但是cur_irq_cnt已经不等于
		//get_current_irq_count_down了，所以直接跳过while，所以就出现了不是以下4种情况的问题
		//暂时解决方法：将cur_irq_cnt == get_current_irq_count_down()换成mem_index != 0或者直接去掉
		while (mem_index != DOWN_QUEUE_AVAILABLE && which_interrput_down == NON_STOP) {
			up(&dev->sem);	
			
			if (wait_event_interruptible(g_irq_wait_queue_down,
							(cur_irq_cnt != get_current_irq_count_down()) || 
							(which_interrput_down != NON_STOP) 
							)) {
				pr_err("down_dev_read wait interrupted exit\n");
				return -ERESTARTSYS;
			}
	
			/* 几种情况：
             * 1.单独dma中断 2.dma后跟打印中断 3.单独打印中断                 4.打印中断后跟dma中断
			 */
			spin_lock_irqsave(&g_irq_lock, flags);
			mem_index = DeQueueF2sm(&g_avail_membuf_queue_down);
			spin_unlock_irqrestore(&g_irq_lock, flags);

			if (down_interruptible(&dev->sem)) {
				pr_err("down_dev_read sem interrupted exit\n");
				return -ERESTARTSYS;
			}
		}
	}

/* situation_1 */
	spin_lock_irqsave(&g_irq_lock, flags);
	if (mem_index == DOWN_QUEUE_AVAILABLE && which_interrput_down == NON_STOP && print_state_down == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		goto ret_to_user_with_dma_interrupt;
	}

/* situation_2 */
	if (mem_index == DOWN_QUEUE_AVAILABLE && which_interrput_down != NON_STOP && print_state_down == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("down_dev_read situation 2\n");
		goto ret_to_user_with_print_interrupt_after_dma_interrupt;
	}

/* situation_3 */
	if (mem_index != DOWN_QUEUE_AVAILABLE && which_interrput_down != NON_STOP && print_state_down == DONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("down_dev_read situation 3\n");
		goto ret_to_user_with_print_interrupt;
	}

/* situation_4 */
	if (mem_index == DOWN_QUEUE_AVAILABLE && which_interrput_down != NON_STOP && print_state_down == UNDONE) {
		spin_unlock_irqrestore(&g_irq_lock, flags);
		pr_info("down_dev_read situation 4\n");
		goto ret_to_user_with_dma_interrupt_after_print_interrupt;	
	}
	spin_unlock_irqrestore(&g_irq_lock, flags);


	/* 不符合以上4种情况的是错误情况 */
	pr_err("down_dev_read not one of four situation\n");
	pr_err("down_dev_read mem_index %d\n", mem_index);
	pr_err("down_dev_read which_interrput_down %d\n", which_interrput_down);
	pr_err("down_dev_read print_state_down %d\n", print_state_down); 
	up(&dev->sem);
	return -ERESTARTSYS;
	

ret_to_user_with_dma_interrupt:
ret_to_user_with_print_interrupt_after_dma_interrupt:
	cur_irq_cnt = get_current_irq_count_down();
	the_f2sm_ram_dev_down.cur_io_buf_index = mem_index;
	cur_read_info.irq_count = cur_irq_cnt;
	cur_read_info.mem_index = mem_index;

	if (copy_to_user(user_buffer, &cur_read_info, count)) {
		pr_err("down_dev_read copy_to_user exit\n");
		up(&dev->sem);
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
		pr_err("down_dev_read not situation 1 or 2, bad situation\n");
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
	the_f2sm_ram_dev_up.cur_io_buf_index = UP_QUEUE_NOT_AVAILABLE;

	which_interrput_up = NON_STOP;
	print_state_up = UNDONE;

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
	EnQueueF2sm(&g_avail_membuf_queue_down, DOWN_QUEUE_AVAILABLE);	
	the_f2sm_ram_dev_down.cur_io_buf_index = DOWN_QUEUE_AVAILABLE;

	which_interrput_down = NON_STOP;
	print_state_down = UNDONE;

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
	the_f2sm_ram_dev_up.cur_io_buf_index = UP_QUEUE_NOT_AVAILABLE;

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
	the_f2sm_ram_dev_down.cur_io_buf_index = DOWN_QUEUE_NOT_AVAILABLE;

	up(&dev->sem);
	
	return 0;
}


/* 用户读取寄存器 */
static long up_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	void __iomem *reg_addr;
	
	user_info_t user_info;
	unsigned long reg_offset;
	unsigned long size;
	void __user * user_addr;

	struct platform_device *pdev;

	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_up;
	if (down_interruptible(&dev->sem)) {
		pr_info("up_dev_ioctl sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	/* 提取用户信息,arg是一个指向user_info_t的指针 */
	ret = copy_from_user(&user_info, (void __user*)arg, sizeof(user_info_t));
	if (ret) {
		pr_err("down_dev_ioctl Copy user arg failed!\n");
		up(&dev->sem);
		return -EFAULT;
	}
	reg_offset = user_info.offset;	//寄存器号
	size = user_info.size;			//需要读写的寄存器内容字节数
	user_addr = user_info.addr;		//需要将寄存器内容写入或读出的用户地址	

	switch (cmd) {
		
	case IOC_CMD_UP_NONE:
		break;
	
	case IOC_CMD_UP_READ:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, reg_offset);
		ret = copy_to_user(user_addr, reg_addr, size);
		if (ret) {
			pr_err("IOC_CMD_UP_READ Copy to user failed!\n");
			up(&dev->sem);
			return -EFAULT;
		}
		break;
		
	case IOC_CMD_UP_WRITE:
		break;

	case IOC_CMD_RESET:
		pdev = the_f2sm_ram_dev_up.pdev;
		ret = device_reset(&pdev->dev);
 		if (ret) {
        	pr_err("failed to reset device\n");
			up(&dev->sem);
			return -EFAULT;
        }	
		break;
	default:
		pr_err("unsupported up ioctl cmd 0x%x!\n", cmd);
		break;
	}

	up(&dev->sem);
	return ret;
}

/* 用户设置寄存器 */
static long down_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	void __iomem *reg_addr;
	
	user_info_t user_info;
	unsigned long reg_offset;
	unsigned long size;
	void __user * user_addr;

	struct platform_device *pdev;
	
	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_down;
	if (down_interruptible(&dev->sem)) {
		pr_info("down_dev_ioctl sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	/* 提取用户信息,arg是一个指向user_info_t的指针 */
	ret = copy_from_user(&user_info, (void __user*)arg, sizeof(user_info_t));
	if (ret) {
		pr_err("down_dev_ioctl Copy user arg failed!\n");
		up(&dev->sem);
		return -EFAULT;
	}
	reg_offset = user_info.offset;	//寄存器号
	size = user_info.size;			//需要读写的寄存器内容字节数
	user_addr = user_info.addr;		//需要将寄存器内容写入或读出的用户地址

	switch (cmd) {
		
	case IOC_CMD_DOWN_NONE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, reg_offset);
		switch (reg_offset) {
		case PRINTER_DATA_MASTER_READ_CONTROL_READ_EN:
		case PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN:
			iowrite32(0x1, reg_addr);
			iowrite32(0x0, reg_addr);			
			break;
		default:
			pr_err("unsupported down ioctl reg 0x%x!\n", cmd);
			break;
		}
		break;
		
	case IOC_CMD_DOWN_READ:
		break;
		
	case IOC_CMD_DOWN_WRITE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, reg_offset);
		ret = copy_from_user(reg_addr, user_addr, size);
		if (ret) {
			pr_err("IOC_CMD_DOWN_WRITE Copy from user failed!\n");
			up(&dev->sem);
			return -EFAULT;
		}		
		break;
		
	case IOC_CMD_RESET:
		pdev = the_f2sm_ram_dev_down.pdev;
		ret = device_reset(&pdev->dev);
 		if (ret) {
        	pr_err("failed to reset device\n");
			up(&dev->sem);
			return -EFAULT;
        }		
		break;
	default:
		pr_err("unsupported down ioctl cmd 0x%x!\n", cmd);
		break;
	}

	up(&dev->sem);
	return ret;
}


#if 0
/* 
 * 读取寄存器信息 
 */
static long up_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	void __iomem *reg_addr;
	unsigned long reg_val;
	unsigned long __user *puser = (unsigned long __user*)arg;

	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_up;
	if (down_interruptible(&dev->sem)) {
		pr_info("up_dev_ioctl sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	switch (cmd) {
	case UP_IOC_FPGA_TYPE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, FPGA_TYPE);
		reg_val = ioread32(reg_addr);
		if (put_user(reg_val, puser)) {
			pr_err("UP_IOC_FPGA_TYPE up_dev_ioctl put_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		break;	
	case UP_IOC_FPGA_DATE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, FPGA_DATE);
		reg_val = ioread32(reg_addr);
		if (put_user(reg_val, puser)) {
			pr_err("UP_IOC_FPGA_DATE up_dev_ioctl put_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		break;
	case UP_IOC_BASE_IMAGE_TRANFER:	
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, I_BASE_IMAGE_TRANFER_COMPLETE);
		reg_val = ioread32(reg_addr);
		if (put_user(reg_val, puser)) {
			pr_err("UP_IOC_BASE_IMAGE_TRANFER up_dev_ioctl put_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		break;	
	default:
		pr_err("unsupported up ioctl cmd 0x%x!\n", cmd);
		break;
	}

	up(&dev->sem);
	
	return ret;
}

/*
 * 配置打印参数
 */
static long down_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	void __iomem *reg_addr;
	unsigned long reg_val;
	unsigned long __user *puser = (unsigned long __user*)arg;

	struct f2sm_ram_dev *dev = &the_f2sm_ram_dev_down;
	if (down_interruptible(&dev->sem)) {
		pr_info("down_dev_ioctl sem interrupted exit\n");
		return -ERESTARTSYS;
	}

	switch (cmd) {
		
	/* prbs和dma */
	case DOWN_IOC_DOWN_PH0_DMA_ADDR:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_DOWN_PH0_DMA_ADDR down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_DOWN_PH1_DMA_ADDR:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_DOWN_PH1_DMA_ADDR down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;			
	case DOWN_IOC_DOWN_BLOCK_SIZE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_DOWN_BLOCK_SIZE down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;			
	case DOWN_IOC_DOWN_DMA_EN:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_READ_CONTROL_READ_EN);
		iowrite32(0x1, reg_addr);
		iowrite32(0x0, reg_addr);
		break;	
	case DOWN_IOC_DOWN_PH0_PRB_SEED:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH0);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_DOWN_PH0_PRB_SEED down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;	
	case DOWN_IOC_DOWN_PH1_PRB_SEED:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH1);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_DOWN_PH1_PRB_SEED down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_UP_PH0_DMA_ADDR:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH0);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_UP_PH0_DMA_ADDR down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_UP_PH1_DMA_ADDR:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH1);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_UP_PH1_DMA_ADDR down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_UP_BLOCK_SIZE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BLOCK_SIZE);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_UP_BLOCK_SIZE down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;			
	case DOWN_IOC_UP_DMA_EN:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN);
		iowrite32(0x1, reg_addr);
		iowrite32(0x0, reg_addr);
		break;		
	case DOWN_IOC_UP_PH0_PRB_SEED:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_SEED_PH0);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_UP_PH0_PRB_SEED down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_UP_PH1_PRB_SEED:					
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_SEED_PH1);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_UP_PH1_PRB_SEED down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;			
		
	/* 打印控制 */
	case DOWN_IOC_PRINT_OPEN:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, PRINT_OPEN);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_PRINT_OPEN down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;
	case DOWN_IOC_LOOP_TEST:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_LOOP_TEST_EN);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_LOOP_TEST down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_DATA_TEST:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_TEST_DATA_EN);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_DATA_TEST down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_MASTER:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_MASTER_INDICATOR);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_MASTER down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		iowrite32(reg_val, reg_addr);
		break;			
 	case DOWN_IOC_SYNC_SIM:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, MASTER_SYNC_SIM_EN);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_SYNC_SIM down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		iowrite32(reg_val, reg_addr);
		break;		
	case DOWN_IOC_BASE_IMAGE:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_BASE_IMAGE_EN);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_BASE_IMAGE down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		iowrite32(reg_val, reg_addr);
		break;	
		
	/* 配置参数 */
	case DOWN_IOC_RASTER_SIM_PARAMS:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_MASTER_RASTER_SEL);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_RASTER_SIM_PARAMS)) {
			pr_err("DOWN_IOC_RASTER_SIM_PARAMS down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}
		break;
	case DOWN_IOC_PD_SIM_PARAMS:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_MASTER_PD_SEL);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_PD_SIM_PARAMS)) {
			pr_err("DOWN_IOC_PD_SIM_PARAMS down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}		
		break;		
	case DOWN_IOC_DIVISOR_PARAMS:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_MASTER_DPI_D_FACTOR);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_DIVISOR_PARAMS)) {
			pr_err("DOWN_IOC_DIVISOR_PARAMS down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}			
		break;			
	case DOWN_IOC_JOB_PARAMS:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_WORK_MODE);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_JOB_PARAMS)) {
			pr_err("DOWN_IOC_JOB_PARAMS down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}		
		break;			
	case DOWN_IOC_PRINT_OFFSET:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_PH_OFFSET);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_PRINT_OFFSET)) {
			pr_err("DOWN_IOC_PRINT_OFFSET down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}				
		break;			
	case DOWN_IOC_FIRE_DELAY:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_DPI_DELAY_NUM_PH0);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_FIRE_DELAY)) {
			pr_err("DOWN_IOC_FIRE_DELAY down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}			
		break;			
	case DOWN_IOC_NOZZLE_SWITCH:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_SW_INIT_EN);
		if (copy_from_user(reg_addr, puser, BYTES_NUM_NOZZLE_SWITCH)) {
			pr_err("DOWN_IOC_NOZZLE_SWITCH down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}		
		break;			
	case DOWN_IOC_SEARCH_LABEL_PARAMS:
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_PD_CHK_THRESHOLD);
		if (get_user(reg_val, puser)) {
			pr_err("DOWN_IOC_SEARCH_LABEL_PARAMS down_dev_ioctl get_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}	
		iowrite32(reg_val, reg_addr);	
		reg_addr = __IO_CALC_ADDRESS_NATIVE(g_ioremap_addr_f2h, O_PD_SERACH_MODE);
		if (copy_from_user(reg_addr, puser + 1, BYTES_NUM_SEARCH_LABEL_PARAMS - 1)) {
			pr_err("DOWN_IOC_SEARCH_LABEL_PARAMS down_dev_ioctl copy_from_user exit\n");
			up(&dev->sem);
			return -EFAULT;
		}		
		break;	

	default:
		pr_err("unsupported down ioctl cmd 0x%x!\n", cmd);
		break;
	}

	up(&dev->sem);
	
	return ret;
}
#endif

static const struct file_operations up_dev_fops = {
	.owner = THIS_MODULE,
	.open = up_dev_open,
	.release = up_dev_release,
	.read = up_dev_read,
	.unlocked_ioctl = up_dev_ioctl,
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
	.unlocked_ioctl = down_dev_ioctl,
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
	priv->the_uio_info.mem[UP_MEM_INDEX].memtype = UIO_MEM_PHYS;
	priv->the_uio_info.mem[UP_MEM_INDEX].addr = res0.start;
	priv->the_uio_info.mem[UP_MEM_INDEX].size = resource_size(&res0);	
	priv->the_uio_info.mem[UP_MEM_INDEX].name = "f2sm_uio_driver_hw_region0";
	priv->the_uio_info.mem[UP_MEM_INDEX].internal_addr = (void *)priv->the_uio_info.mem[UP_MEM_INDEX].addr;
	g_ioremap_addr_up = ioremap(priv->the_uio_info.mem[UP_MEM_INDEX].addr, 2 * priv->the_uio_info.mem[UP_MEM_INDEX].size); 
	if (g_ioremap_addr_up == NULL) {
		pr_err("ioremap failed: g_ioremap_addr_up\n");
		goto bad_exit_release_mem_region;
	}
	dev_info(&pdev->dev, "Allocated reserved memory, vaddr: 0x%p, paddr: 0x%08X\n", g_ioremap_addr_up, priv->the_uio_info.mem[UP_MEM_INDEX].addr);
	priv->the_uio_info.mem[UP_MEM_INDEX].internal_addr = g_ioremap_addr_up;

	/* 下行 */
	priv->the_uio_info.mem[DOWN_MEM_INDEX].memtype = UIO_MEM_PHYS;
	priv->the_uio_info.mem[DOWN_MEM_INDEX].addr = res1.start;
	priv->the_uio_info.mem[DOWN_MEM_INDEX].size = resource_size(&res1);
	priv->the_uio_info.mem[DOWN_MEM_INDEX].name = "f2sm_uio_driver_hw_region1";
	priv->the_uio_info.mem[DOWN_MEM_INDEX].internal_addr = (void *)priv->the_uio_info.mem[DOWN_MEM_INDEX].addr;
	g_ioremap_addr_down = ioremap(priv->the_uio_info.mem[DOWN_MEM_INDEX].addr, 2 * priv->the_uio_info.mem[DOWN_MEM_INDEX].size); 
	if (g_ioremap_addr_down == NULL) {
		pr_err("ioremap failed: g_ioremap_addr_down\n");		
		goto bad_exit_iounmap;
	}
	dev_info(&pdev->dev, "Allocated reserved memory, vaddr: 0x%p, paddr: 0x%08X\n", g_ioremap_addr_down, priv->the_uio_info.mem[1].addr);
	priv->the_uio_info.mem[DOWN_MEM_INDEX].internal_addr = g_ioremap_addr_down;

	/* 映射f2h桥 */ 
	g_ioremap_addr_f2h = ioremap(MMAP_BASE, MMAP_SPAN);
	if (g_ioremap_addr_f2h == NULL) {
		pr_err("ioremap failed: g_ioremap_addr_f2h\n");
		goto bad_exit_iounmap;
	}	

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

	/* 文件私有数据和平台设备及其私有数据关联起来 */
	the_f2sm_ram_dev_down.p_uio_info = &priv->the_uio_info;
	the_f2sm_ram_dev_down.pdev = pdev;	
	the_f2sm_ram_dev_up.p_uio_info = &priv->the_uio_info;
	the_f2sm_ram_dev_up.pdev = pdev;


	/* 注册杂项设备 */
	ret_val = misc_register(&up_dev_device);
	if (ret_val != 0) {
		pr_err("Could not register device \"up_dev\"...");
		goto bad_exit_freeirq0_1_4_5;
	}
	ret_val = misc_register(&down_dev_device);
	if (ret_val != 0) {
		pr_err("Could not register device \"down_dev\"...");
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
	if (g_ioremap_addr_f2h) {
		iounmap(g_ioremap_addr_f2h);
        g_ioremap_addr_f2h = NULL;
    }	
	if (g_ioremap_addr_down) {
		iounmap(g_ioremap_addr_down);
        g_ioremap_addr_down = NULL;
    }	
	if (g_ioremap_addr_up) {
		iounmap(g_ioremap_addr_up);
        g_ioremap_addr_up = NULL;
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
	
	if (g_ioremap_addr_f2h) {
		iounmap(g_ioremap_addr_f2h);
        g_ioremap_addr_f2h = NULL;
    }	
	if (g_ioremap_addr_down) {
		iounmap(g_ioremap_addr_down);
        g_ioremap_addr_down = NULL;
    }	
	if (g_ioremap_addr_up) {
		iounmap(g_ioremap_addr_up);
        g_ioremap_addr_up = NULL;
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

static struct of_device_id print_driver_dt_ids[] = {
	{ .compatible = "print_dev,driver-1.0"},
	{ /* end of table */ }
};


MODULE_DEVICE_TABLE(of, print_driver_dt_ids);

static struct platform_driver the_platform_driver = {
	.probe = platform_probe,
	.remove = platform_remove,
	.driver = {
		   .name = "print_dev_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = print_driver_dt_ids,
		   },

};

static int print_init(void)
{
	int ret_val;
	pr_info("print_dev_init enter\n");

	sema_init(&g_dev_probe_sem, 1);

	ret_val = platform_driver_register(&the_platform_driver);
	if (ret_val != 0) {
		pr_err("platform_driver_register returned %d\n", ret_val);
		return ret_val;
	}

	pr_info("print_dev_init exit\n");
	return 0;
}

static void print_exit(void)
{
	pr_info("print_dev_exit enter\n");

	platform_driver_unregister(&the_platform_driver);

	pr_info("print_dev_exit exit\n");
}

module_init(print_init);
module_exit(print_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Student One <dso@company.com>");
MODULE_DESCRIPTION("Demonstration Module 5 - reserve memory and enable IRQ");

