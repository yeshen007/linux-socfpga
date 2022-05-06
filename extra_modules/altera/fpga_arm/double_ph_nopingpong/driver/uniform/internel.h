#ifndef _INTERNEL_H_
#define _INTERNEL_H_


#include "alt_f2sm_regs.h"
#include "fpga_regs.h"


struct f2sm_buf_q
{
	int mem[4];   
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


/* 
 * 以下是ioctl使用的宏
 */
#define DOWN_MAGIC	(('d' + 'o' + 'w' + 'n') & 0xff)	//down_dev_ioctl魔数
#define UP_MAGIC	(('u' + 'p') & 0xff)				//up_dev_ioctl魔数
#define RESET_MAGIC	(('r' + 'e' + 's' + 'e' + 't') & 0xff)	//复位魔数


/* 上行cmd */
#define IOC_CMD_UP_NONE		_IOC(_IOC_NONE,UP_MAGIC,0,0)
#define IOC_CMD_UP_READ		_IOC(_IOC_READ,UP_MAGIC,0,0)
#define IOC_CMD_UP_WRITE	_IOC(_IOC_WRITE,UP_MAGIC,0,0)

/* 下行cmd */
#define IOC_CMD_DOWN_NONE	_IOC(_IOC_NONE,DOWN_MAGIC,0,0)
#define IOC_CMD_DOWN_READ	_IOC(_IOC_READ,DOWN_MAGIC,0,0)
#define IOC_CMD_DOWN_WRITE	_IOC(_IOC_WRITE,DOWN_MAGIC,0,0)

/* 复位 */
#define IOC_CMD_RESET	_IOC(_IOC_NONE,RESET_MAGIC,0,0)


typedef struct user_info {
	unsigned long offset;
	unsigned long size;
	void __user * addr;
} user_info_t;


#if 0
/* dma和prbs功能号 */
#define NR_DOWN_PH0_DMA_ADDR	0x1		
#define NR_DOWN_PH1_DMA_ADDR	0x2
#define NR_DOWN_BLOCK_SIZE		0x3
#define NR_DOWN_DMA_EN			0x4
#define NR_DOWN_PH0_PRB_SEED	0x5
#define NR_DOWN_PH1_PRB_SEED	0x6
#define NR_UP_PH0_DMA_ADDR		0x7	
#define NR_UP_PH1_DMA_ADDR		0x8
#define NR_UP_BLOCK_SIZE		0x9
#define NR_UP_DMA_EN			0xa
#define NR_UP_PH0_PRB_SEED		0xb
#define NR_UP_PH1_PRB_SEED		0xc
/* 打印控制功能号 */
#define NR_PRINT_OPEN			0x41
#define NR_LOOP_TEST			0x42
#define NR_DATA_TEST			0x43
#define NR_MASTER				0x44
#define NR_SYNC_SIM				0x45
#define NR_BASE_IMAGE			0x46 
/* 配置参数功能号 */
#define NR_RASTER_SIM_PARAMS	0x81
#define NR_PD_SIM_PARAMS		0x82
#define NR_DIVISOR_PARAMS		0x83
#define NR_JOB_PARAMS			0x84
#define NR_PRINT_OFFSET			0x85
#define NR_FIRE_DELAY			0x86
#define NR_NOZZLE_SWITCH		0x87
#define NR_SEARCH_LABEL_PARAMS	0x88
/* 返回信息功能号 */
#define NR_FPGA_TYPE			0x1
#define NR_FPGA_DATE			0x2
#define NR_BASE_IMAGE_TRANFER	0x3


/* dma和prbs的cmd */
#define DOWN_IOC_DOWN_PH0_DMA_ADDR	 _IOW(DOWN_MAGIC,NR_DOWN_PH0_DMA_ADDR,unsigned long)		
#define DOWN_IOC_DOWN_PH1_DMA_ADDR	 _IOW(DOWN_MAGIC,NR_DOWN_PH1_DMA_ADDR,unsigned long)
#define DOWN_IOC_DOWN_BLOCK_SIZE	 _IOW(DOWN_MAGIC,NR_DOWN_BLOCK_SIZE,unsigned long)
#define DOWN_IOC_DOWN_DMA_EN		 _IOW(DOWN_MAGIC,NR_DOWN_DMA_EN,unsigned long)
#define DOWN_IOC_DOWN_PH0_PRB_SEED	 _IOW(DOWN_MAGIC,NR_DOWN_PH0_PRB_SEED,unsigned long)
#define DOWN_IOC_DOWN_PH1_PRB_SEED	 _IOW(DOWN_MAGIC,NR_DOWN_PH1_PRB_SEED,unsigned long)
#define DOWN_IOC_UP_PH0_DMA_ADDR	 _IOW(DOWN_MAGIC,NR_UP_PH0_DMA_ADDR,unsigned long)		
#define DOWN_IOC_UP_PH1_DMA_ADDR	 _IOW(DOWN_MAGIC,NR_UP_PH1_DMA_ADDR,unsigned long)
#define DOWN_IOC_UP_BLOCK_SIZE		 _IOW(DOWN_MAGIC,NR_UP_BLOCK_SIZE,unsigned long)
#define DOWN_IOC_UP_DMA_EN			 _IOW(DOWN_MAGIC,NR_UP_DMA_EN,unsigned long)
#define DOWN_IOC_UP_PH0_PRB_SEED	 _IOW(DOWN_MAGIC,NR_UP_PH0_PRB_SEED,unsigned long)
#define DOWN_IOC_UP_PH1_PRB_SEED	 _IOW(DOWN_MAGIC,NR_UP_PH1_PRB_SEED,unsigned long)
/* 打印控制cmd */
#define DOWN_IOC_PRINT_OPEN			 _IOW(DOWN_MAGIC,NR_PRINT_OPEN,unsigned long) 
#define DOWN_IOC_LOOP_TEST			 _IOW(DOWN_MAGIC,NR_LOOP_TEST,unsigned long)
#define DOWN_IOC_DATA_TEST			 _IOW(DOWN_MAGIC,NR_DATA_TEST,unsigned long)
#define DOWN_IOC_MASTER				 _IOW(DOWN_MAGIC,NR_MASTER,unsigned long) 
#define DOWN_IOC_SYNC_SIM			 _IOW(DOWN_MAGIC,NR_SYNC_SIM,unsigned long)
#define DOWN_IOC_BASE_IMAGE			 _IOW(DOWN_MAGIC,NR_BASE_IMAGE,unsigned long)
/* 配置参数cmd */
#define DOWN_IOC_RASTER_SIM_PARAMS	 _IOW(DOWN_MAGIC,NR_RASTER_SIM_PARAMS,unsigned long)
#define DOWN_IOC_PD_SIM_PARAMS		 _IOW(DOWN_MAGIC,NR_PD_SIM_PARAMS,unsigned long)
#define DOWN_IOC_DIVISOR_PARAMS		 _IOW(DOWN_MAGIC,NR_DIVISOR_PARAMS,unsigned long)
#define DOWN_IOC_JOB_PARAMS			 _IOW(DOWN_MAGIC,NR_JOB_PARAMS,unsigned long)
#define DOWN_IOC_PRINT_OFFSET		 _IOW(DOWN_MAGIC,NR_PRINT_OFFSET,unsigned long)
#define DOWN_IOC_FIRE_DELAY			 _IOW(DOWN_MAGIC,NR_FIRE_DELAY,unsigned long)
#define DOWN_IOC_NOZZLE_SWITCH		 _IOW(DOWN_MAGIC,NR_NOZZLE_SWITCH,unsigned long)
#define DOWN_IOC_SEARCH_LABEL_PARAMS _IOW(DOWN_MAGIC,NR_SEARCH_LABEL_PARAMS,unsigned long)
/* 返回信息cmd */
#define UP_IOC_FPGA_TYPE			 _IOR(UP_MAGIC,NR_FPGA_TYPE,unsigned long)
#define UP_IOC_FPGA_DATE			 _IOR(UP_MAGIC,NR_FPGA_DATE,unsigned long)
#define UP_IOC_BASE_IMAGE_TRANFER	 _IOR(UP_MAGIC,NR_BASE_IMAGE_TRANFER,unsigned long)


/* 功能对应的寄存器数量,由xx_dev_ioctl根据cmd判断使用,只列出大于1的,其他的都为1 */
#define REGS_NUM_RASTER_SIM_PARAMS		2
#define REGS_NUM_PD_SIM_PARAMS			5
#define REGS_NUM_DIVISOR_PARAMS 		3
#define REGS_NUM_JOB_PARAMS				5
#define REGS_NUM_PRINT_OFFSET			9
#define REGS_NUM_FIRE_DELAY				4
#define REGS_NUM_NOZZLE_SWITCH			6
#define REGS_NUM_SEARCH_LABEL_PARAMS	5

/* 功能对应的读出或写入的字节数,未列出的都为sizeof(unsigned long) */
#define BYTES_NUM_RASTER_SIM_PARAMS		(REGS_NUM_RASTER_SIM_PARAMS*sizeof(unsigned long))
#define BYTES_NUM_PD_SIM_PARAMS			(REGS_NUM_PD_SIM_PARAMS*sizeof(unsigned long))
#define BYTES_NUM_DIVISOR_PARAMS 		(REGS_NUM_DIVISOR_PARAMS*sizeof(unsigned long))
#define BYTES_NUM_JOB_PARAMS			(REGS_NUM_JOB_PARAMS*sizeof(unsigned long))
#define BYTES_NUM_PRINT_OFFSET			(REGS_NUM_PRINT_OFFSET*sizeof(unsigned long))
#define BYTES_NUM_FIRE_DELAY			(REGS_NUM_FIRE_DELAY*sizeof(unsigned long))
#define BYTES_NUM_NOZZLE_SWITCH			(REGS_NUM_NOZZLE_SWITCH*sizeof(unsigned long))
#define BYTES_NUM_SEARCH_LABEL_PARAMS	(REGS_NUM_SEARCH_LABEL_PARAMS*sizeof(unsigned long))
#endif

#endif

