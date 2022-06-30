#ifndef _BASE_PRINT_H_
#define _BASE_PRINT_H_

#include "fpga_regs.h"
#include "utils.h"


#define F2SM_MEM_NUMS 2

#define UP_DEV	 "/dev/up_dev"				//上行设置节点
#define DOWN_DEV "/dev/down_dev"			//下行设置节点

//#define MEM_PHY_UP 0x30000000				//上行dma buffer物理地址,ph0 0x30000000,ph1 0x31000000
//#define MEM_PHY_UP_SIZE 0x02000000		//上行dma buffer大小,ph0和ph1各0x01000000
//#define MEM_PHY_DOWN 0x32000000			//下行dma buffer物理地址,ph0 0x32000000,ph1 0x33000000
//#define MEM_PHY_DOWN_SIZE 0x02000000		//下行dma buffer大小,ph0和ph1各0x01000000
#define PH1_OFFSET 0x01000000				//dma buffer中ph0部分和ph1部分的偏移

#define UP_MEM_INDEX 	0
#define DOWN_MEM_INDEX 	1

typedef struct f2sm_mem_info {
    void* raw_addr; 	
    int   mem_size;
} f2sm_mem_info_t;

typedef struct print_info {
	int up_fd;									//上行设备文件/dev/up_dev文件描述符
	int down_fd;								//下行设备文件/dev/down_dev文件描述符
	f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];	//上行和下行dma buffer物理地址和大小

	void (*print_init)(struct print_info *print_info);	//打开设备文件并赋给up_fd、down_fd,获取设置dma地址信息mem_info
	void (*print_close)(struct print_info *print_info);	//释放up_fd、down_fd、mem_info
	void (*print_stop)(struct print_info *print_info);	//软件主动退出阻塞
	void (*print_reset)(struct print_info *print_info);	//软件复位fpga
	void (*print_nonblock_up)(struct print_info *print_info);	//设置上行为非阻塞,默认为阻塞模式
	void (*print_nonblock_down)(struct print_info *print_info);	//设置下行为非阻塞,默认为阻塞模式

	/* prbs和dma */
	//starttransfer_up设置ph0和ph1的上行dma物理地址和大小,然后启动上行dma
	//starttransfer_down设置ph0和ph1的下行dma物理地址和大小,然后启动上行dma
	//starttransfer_down_seed设置ph0和ph1的下行dma物理地址和大小和prbs种子,然后启动下行dma
	void (*starttransfer_up)(struct print_info *print_info, unsigned int blk_count); 
	void (*starttransfer_down)(struct print_info *print_info, unsigned int blk_count);
	void (*starttransfer_down_seed)(struct print_info *print_info,
									unsigned long seed1, 
									unsigned long seed2, 
									unsigned long blk_count);

	/* 打印相关控制 */
	void (*print_enable)(struct print_info *print_info);		//开始打印
	void (*print_disable)(struct print_info *print_info);		//结束打印
	void (*config_master)(struct print_info *print_info);		//配置为主板
	void (*config_slave)(struct print_info *print_info);		//配置为从板
	void (*sync_sim_enable)(struct print_info *print_info);		//使能硬件仿真
	void (*test_data_enable)(struct print_info *print_info);	//使能HPS测试数据模式
	void (*loop_test_enable)(struct print_info *print_info);	//使能HPS DMA上行环回	
	void (*base_image_enable)(struct print_info *print_info);	//开始基图发送
	void (*base_image_disable)(struct print_info *print_info);	//结束基图发送
	void (*base_image_tranfer_complete)(struct print_info *print_info);	//轮询等待基图发送完成
	void (*get_borad_id)(struct print_info *print_info, unsigned long *id);		//获取borad id

	/* 打印相关参数 */
	void (*config_raster_sim_params)(struct print_info *print_info, unsigned long *regs);	//配置raster仿真参数
	void (*config_pd_sim_params)(struct print_info *print_info, unsigned long *regs);		//配置pd仿真参数
	void (*config_divisor_params)(struct print_info *print_info, unsigned long *regs);		//配置分频系数
	void (*config_job_params)(struct print_info *print_info, unsigned long *regs);			//配置作业参数
	void (*config_print_offset)(struct print_info *print_info, unsigned long *regs);		//配置打印偏置
	void (*config_fire_delay)(struct print_info *print_info, unsigned long *regs);			//配置点火延时
	void (*config_nozzle_switch)(struct print_info *print_info, unsigned long *regs);		//配置喷孔开关
	void (*config_search_label_params)(struct print_info *print_info, unsigned long *regs);	//配置搜标参数
} print_info_t;


void init_print_info(struct print_info *print_info);		//初始化print_info的打印函数和数据成员up_fd,down_fd,mem_info 
void close_print_info(struct print_info *print_info);	//print_info的打印函数设置为NULL,同时释放up_fd,down_fd

#endif
