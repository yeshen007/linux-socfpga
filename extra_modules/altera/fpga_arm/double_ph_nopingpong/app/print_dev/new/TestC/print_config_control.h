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
	int up_fd;
	int down_fd;
	int mem_fd;
	void *virtual_base;
	f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];

	void (*print_init)(struct print_info *print_info);	
	void (*print_close)(struct print_info *print_info);	

	/* 以下三部分函数在新接口中使用ioctl替代 */
	void (*starttransfer_up)(struct print_info *print_info, int up_mem_index, unsigned int blk_count); 
	void (*starttransfer_down)(struct print_info *print_info, int down_mem_index, unsigned int blk_count);
	void (*starttransfer_down_seed)(struct print_info *print_info, unsigned long seed1, unsigned long seed2, unsigned long blk_count);
	
	void (*print_enable)(struct print_info *print_info);
	void (*print_disable)(struct print_info *print_info);
	void (*config_master)(struct print_info *print_info);
	void (*config_slave)(struct print_info *print_info);
	void (*sync_sim_enable)(struct print_info *print_info);
	void (*test_data_enable)(struct print_info *print_info);
	void (*loop_test_enable)(struct print_info *print_info);
	void (*base_image_enable)(struct print_info *print_info);
	void (*base_image_disable)(struct print_info *print_info);

	void (*config_raster_sim_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_pd_sim_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_divisor_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_job_params)(struct print_info *print_info, unsigned long *regs);
	void (*config_print_offset)(struct print_info *print_info, unsigned long *regs);
	void (*config_fire_delay)(struct print_info *print_info, unsigned long *regs);
	void (*config_nozzle_switch)(struct print_info *print_info, unsigned long *regs);	
	void (*config_search_label_params)(struct print_info *print_info, unsigned long *regs);
} print_info_t;

void init_print_info(struct print_info *print_info);


#endif
