#ifndef _BASE_PRINT_H_
#define _BASE_PRINT_H_


#define F2SM_MEM_NUMS 2

#define UP_DEV	 "/dev/up_dev"
#define DOWN_DEV "/dev/down_dev"

#define MEM_PHY_UP 0x30000000
#define MEM_PHY_UP_SIZE 0x02000000
#define MEM_PHY_DOWN 0x32000000
#define MEM_PHY_DOWN_SIZE 0x02000000
#define PH1_OFFSET 0x01000000

#define UP_MEM_INDEX 	0
#define DOWN_MEM_INDEX 	1

typedef struct f2sm_mem_info {
    void* raw_addr; 
    void* mem_addr; 
    int   mem_size;
} f2sm_mem_info_t;

typedef struct print_info {
	int up_fd;				//上行设备文件/dev/up_dev文件描述符
	int down_fd;			//下行设备文件/dev/down_dev文件描述符
	int mem_fd;				//在ioctl方式中弃用
	void *virtual_base;		//在ioctl方式中弃用
	f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];	//上行和下行dma buffer物理地址

	void (*print_init)(struct print_info *print_info);	//设置up_fd、down_fd、mem_info
	void (*print_close)(struct print_info *print_info);	//释放up_fd、down_fd、mem_info


	/* prbs和dma */
	//设置ph0和ph1的上行dma物理地址和大小,然后启动上行dma
	void (*starttransfer_up)(struct print_info *print_info, int up_mem_index, unsigned int blk_count); 
	//设置ph0和ph1的下行dma物理地址和大小,然后启动上行dma
	void (*starttransfer_down)(struct print_info *print_info, int down_mem_index, unsigned int blk_count);
	//设置ph0和ph1的下行dma物理地址和大小和prbs种子,然后启动下行dma
	void (*starttransfer_down_seed)(struct print_info *print_info, unsigned long seed1, unsigned long seed2, unsigned long blk_count);

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

	/* 打印相关参数 */
	void (*config_raster_sim_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_pd_sim_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_divisor_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_job_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_print_offset)(struct print_info *print_info, unsigned long *regs);
	void (*config_fire_delay)(struct print_info *print_info, unsigned long *regs);
	void (*config_nozzle_switch)(struct print_info *print_info, unsigned long *regs);	
	void (*config_search_label_params)(struct print_info *print_info, unsigned long *regs);
} print_info_t;


//设置好print_info的打印函数
//设置print_info的up_fd、down_fd、mem_info为默认值，需要进一步调用print_info.print_init
void init_print_info(struct print_info *print_info);


#endif
