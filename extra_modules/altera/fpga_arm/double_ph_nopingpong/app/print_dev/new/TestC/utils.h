#ifndef _UTILS_H_
#define _UTILS_H_

#define HAN_EINTR 4
#define HAN_ENOMEM 12
#define HAN_EFAULT 14
#define HAN_EINVAL 22    
#define HAN_ERESTARTSYS 512 

#define HAN_E_SUCCESS 	0				//成功打开文件或者ioctl成功传输
#define HAN_E_PT_FIN 	HAN_E_SUCCESS   //收到fpga的打印完成中断 
#define HAN_E_PT_EXPT 	(-HAN_EFAULT)    	//收到fpga的打印异常中断
#define HAN_E_INT		(-HAN_EINTR)  		//被信号中断
#define HAN_E_BAD_ADDR  (-HAN_ENOMEM)		//传入非法地址
#define HAN_E_BAD_ARG   (-HAN_EINVAL)  		//传入其他不正确的参数
#define HAN_E_SYS_ERR   (-HAN_ERESTARTSYS)  //驱动bug,需要进一步调试定位



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


/* ioctl cmd字段解析,统一使用下行的fd来ioctl
 * cmd组成:
 * dir  -- 读写控制位
 		   10表示用户读寄存器
 		   01表示用户写寄存器
 		   00表示不需要将寄存器读给用户也不需要从用户传入寄存器值进来,
 		     用来设置一些固定寄存器值,或者读取一些寄存器值但不用返回给用户
 * type -- 一般是某种魔数,我们使用下文的HAN_MAGIC
 * nr   -- 0x1表示读写或设置普通的fpga寄存器
           0x2表示设置fpga复位寄存器使fpga复位
 * size -- 保留不用
+++++++++++++++++++++++++++++++++++++++++++++++
|dir(2bits)|type(8bits)|nr(8bits)|size(14bits)|
+++++++++++++++++++++++++++++++++++++++++++++++
*/

//dir
#define HAN_DIR_NONE		0U
#define HAN_DIR_WRITE		1U
#define HAN_DIR_READ		2U

//type
#define HAN_TYPE_MAGIC	(('h' + 'a' + 'n') & 0xff)	

//nr
#define HAN_NR_NORMAL		0x1
#define HAN_NR_RESET		0x2
#define HAN_NR_PH_DMA		0x3


#define HAN_IOC_NRBITS		8
#define HAN_IOC_TYPEBITS	8
#define HAN_IOC_SIZEBITS	14
#define HAN_IOC_DIRBITS		2

#define HAN_IOC_NRSHIFT		0		
#define HAN_IOC_TYPESHIFT	(HAN_IOC_NRSHIFT+HAN_IOC_NRBITS)			//8
#define HAN_IOC_SIZESHIFT	(HAN_IOC_TYPESHIFT+HAN_IOC_TYPEBITS)		//16
#define HAN_IOC_DIRSHIFT	(HAN_IOC_SIZESHIFT+HAN_IOC_SIZEBITS)		//30


#define HAN_IOC(dir,type,nr,size) \
		(((dir)  << HAN_IOC_DIRSHIFT) | \
		 ((type) << HAN_IOC_TYPESHIFT) | \
		 ((nr)	 << HAN_IOC_NRSHIFT) | \
		 ((size) << HAN_IOC_SIZESHIFT))
	

#define HAN_IO(nr)		HAN_IOC(HAN_DIR_NONE,HAN_TYPE_MAGIC,nr,0)
#define HAN_IOR(nr)		HAN_IOC(HAN_DIR_READ,HAN_TYPE_MAGIC,nr,0)
#define HAN_IOW(nr)		HAN_IOC(HAN_DIR_WRITE,HAN_TYPE_MAGIC,nr,0)


/* cmd */
#define IOC_CMD_NONE	HAN_IO(HAN_NR_NORMAL) 		//设置寄存器固定值或者读取寄存器但不传给用户
#define IOC_CMD_READ	HAN_IOR(HAN_NR_NORMAL) 		//读取寄存器值给用户
#define IOC_CMD_WRITE	HAN_IOW(HAN_NR_NORMAL)		//设置用户传入的寄存器值
#define IOC_CMD_RESET	HAN_IO(HAN_NR_RESET)		//设置复位寄存器使fpga复位
#define IOC_CMD_PH_DMA	HAN_IOR(HAN_NR_PH_DMA)		//用户获取ph的dma buffer物理地址信息


typedef struct arg_info {
	unsigned long offset;		//寄存器偏移
	unsigned long size;			//用户需要读写的寄存器字节数
	void *addr;					//需要写入寄存器的变量地址或者读出寄存器存放的变量地址
} arg_info_t;


#define HALF_REG_SIZE 	 2
#define ONE_REG_SIZE 	 4
#define NO_NEED_ARG_SIZE 0

#define NO_NEED_ARG_ADDR NULL
#define NO_NEED_OFFSET	 0XFFFFFFFF


int Ioctl(int fd, unsigned long cmd, void *arg);


#endif
