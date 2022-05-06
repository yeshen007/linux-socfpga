#ifndef _UTILS_H_
#define _UTILS_H_

#include "fpga_regs.h"

typedef unsigned char u8;
typedef unsigned int u32;

#define TEST_BLOCK_SIZE (3*1024)
#define TEST_BLOCK_CNT 1024
#define PH1_USERBUF_OFFSET 	(16*1024*1024)

struct f2sm_read_info_t {
	u32 irq_count;
	int mem_index;
};


int build_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt);
int check_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt);


/* cmd组成:
 * dir  -- 读写控制位
 		   10表示用户读寄存器
 		   01表示用户写寄存器
 		   00表示不需要将寄存器读给用户也不需要从用户传入寄存器值进来,用来设置一些固定寄存器值
 * type -- 一般是某种魔数,
           下行ioctl使用DOWN_MAGIC
           上行ioctl使用UP_MAGIC
 * nr   -- 0,放到arg
 * size -- 0,放到arg
+++++++++++++++++++++++++++++++++++++++++++++++
|dir(2bits)|type(8bits)|nr(8bits)|size(14bits)|
+++++++++++++++++++++++++++++++++++++++++++++++
*/

#define DOWN_MAGIC	(('d' + 'o' + 'w' + 'n') & 0xff)		//down_dev_ioctl魔数
#define UP_MAGIC	(('u' + 'p') & 0xff)					//up_dev_ioctl魔数
#define RESET_MAGIC	(('r' + 'e' + 's' + 'e' + 't') & 0xff)	//复位魔数

#define HAN_IOC_NRBITS	8
#define HAN_IOC_TYPEBITS	8
#define HAN_IOC_SIZEBITS	14
#define HAN_IOC_DIRBITS	2

#define HAN_IOC_NRSHIFT	0		
#define HAN_IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)			//8
#define HAN_IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)		//16
#define HAN_IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)		//30

#define HAN_IOC_NONE	0U
#define HAN_IOC_WRITE	1U
#define HAN_IOC_READ	2U

#define HAN_IOC(dir,type,nr,size) \
		(((dir)  << _IOC_DIRSHIFT) | \
		 ((type) << _IOC_TYPESHIFT) | \
		 ((nr)	 << _IOC_NRSHIFT) | \
		 ((size) << _IOC_SIZESHIFT))
	

#define HAN_IO(type)	HAN_IOC(_IOC_NONE,(type),0,0)
#define HAN_IOR(type)	HAN_IOC(_IOC_READ,(type),0,0)
#define HAN_IOW(type)	HAN_IOC(_IOC_WRITE,(type),0,0)


/* 上行cmd */
#define IOC_CMD_UP_NONE		HAN_IO(UP_MAGIC)
#define IOC_CMD_UP_READ		HAN_IOR(UP_MAGIC) 
#define IOC_CMD_UP_WRITE	HAN_IOW(UP_MAGIC)

/* 下行cmd */
#define IOC_CMD_DOWN_NONE	HAN_IO(DOWN_MAGIC)
#define IOC_CMD_DOWN_READ	HAN_IOR(DOWN_MAGIC) 
#define IOC_CMD_DOWN_WRITE	HAN_IOW(DOWN_MAGIC)

/* 复位,上行下行ioctl都可复位 */
#define IOC_CMD_RESET	HAN_IO(RESET_MAGIC)


typedef struct arg_info {
	unsigned long offset;
	unsigned long size;
	void *addr;
} arg_info_t;

#define HALF_REG_SIZE 2
#define ONE_REG_SIZE 4

#endif
