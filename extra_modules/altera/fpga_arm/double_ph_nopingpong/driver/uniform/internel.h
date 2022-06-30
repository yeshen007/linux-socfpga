#ifndef _INTERNEL_H_
#define _INTERNEL_H_


#include "alt_f2sm_regs.h"
#include "fpga_regs.h"


struct f2sm_buf_q
{
	int size;   
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
	unsigned long count;
    struct uio_info *p_uio_info;
	int cur_io_buf_index;
	struct platform_device *pdev;
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


#define UP_MEM_INDEX 0 
#define DOWN_MEM_INDEX 1 

#define QUEUE_AVAILABLE 0
#define QUEUE_NOT_AVAILABLE -1

#define UP_QUEUE_AVAILABLE QUEUE_AVAILABLE
#define UP_QUEUE_NOT_AVAILABLE QUEUE_NOT_AVAILABLE
#define DOWN_QUEUE_AVAILABLE QUEUE_AVAILABLE
#define DOWN_QUEUE_NOT_AVAILABLE QUEUE_NOT_AVAILABLE


#define NON_STOP 0
#define NORMAL_STOP 1
#define ACCIDENT_STOP 2
#define DONE 1
#define UNDONE 0
#define PH1_USERBUF_OFFSET 	(16*1024*1024)
#define PH1_OFFSET 	0x01000000
#define IS_F2SM_QUEUE_FULL(buf) (((buf->tail + 1) & 0x3) == buf->head) 


#define MEM_PHY_UP 0x30000000				//上行dma buffer物理地址,ph0 0x30000000,ph1 0x31000000
#define MEM_PHY_UP_SIZE 0x02000000			//上行dma buffer大小,ph0和ph1各0x01000000
#define MEM_PHY_DOWN 0x32000000				//下行dma buffer物理地址,ph0 0x32000000,ph1 0x33000000
#define MEM_PHY_DOWN_SIZE 0x02000000		//下行dma buffer大小,ph0和ph1各0x01000000


/* 
 * 以下是ioctl使用的宏
 */
 //type
#define HAN_TYPE_MAGIC	(('h' + 'a' + 'n') & 0xff)

//nr
#define HAN_NR_FPGA_REG		0x1
#define HAN_NR_RESET_FPGA	0x2
#define HAN_NR_PH_DMA		0x3
#define HAN_NR_SOFT_STOP	0x4
#define HAN_NR_END_ONE		0x5


//cmd
#if 0
#define IOC_CMD_NONE	_IOC(_IOC_NONE,HAN_TYPE_MAGIC,HAN_NR_FPGA_REG,0)		//设置寄存器固定值或者读取寄存器但不传给用户
#endif
#define IOC_CMD_READ	_IOC(_IOC_READ,HAN_TYPE_MAGIC,HAN_NR_FPGA_REG,0)		//读取寄存器值给用户
#define IOC_CMD_WRITE	_IOC(_IOC_WRITE,HAN_TYPE_MAGIC,HAN_NR_FPGA_REG,0)		//设置用户传入的寄存器值
#define IOC_CMD_RESET	_IOC(_IOC_NONE,HAN_TYPE_MAGIC,HAN_NR_RESET_FPGA,0)		//设置复位寄存器使fpga复位
#define IOC_CMD_PH_DMA	_IOC(_IOC_READ,HAN_TYPE_MAGIC,HAN_NR_PH_DMA,0)			//用户获取ph的dma buffer物理地址信息
#define IOC_CMD_STOP	_IOC(_IOC_NONE,HAN_TYPE_MAGIC,HAN_NR_SOFT_STOP,0)		//退出阻塞
#define IOC_CMD_END_ONE	_IOC(_IOC_NONE,HAN_TYPE_MAGIC,HAN_NR_END_ONE,0)			//结束一单


typedef struct user_info {
	unsigned long offset;
	unsigned long size;
	void __user * addr;
} user_info_t;



#endif

