#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>


#include "print_config_control.h"
#include "utils.h"


/* 
 * ioctl相关
 */
#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8
#define _IOC_SIZEBITS	14
#define _IOC_DIRBITS	2

#define _IOC_NRSHIFT	0		
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)			//8
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)		//16
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)		//30

#define _IOC_NONE	0U
#define _IOC_WRITE	1U
#define _IOC_READ	2U

#define _IOC(dir,type,nr,size) \
		(((dir)  << _IOC_DIRSHIFT) | \
		 ((type) << _IOC_TYPESHIFT) | \
		 ((nr)	 << _IOC_NRSHIFT) | \
		 ((size) << _IOC_SIZESHIFT))
	
#define _IOC_TYPECHECK(t) (sizeof(t))
	
#define _IO(type,nr)		_IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)	_IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOW(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOWR(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOR_BAD(type,nr,size)	_IOC(_IOC_READ,(type),(nr),sizeof(size))
#define _IOW_BAD(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWR_BAD(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))

#define DOWN_MAGIC	(('d' + 'o' + 'w' + 'n') & 0xff)	//down_dev_ioctl魔数
#define UP_MAGIC	(('u' + 'p') & 0xff)				//up_dev_ioctl魔数

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


#define NO_NEED_IOC_ARG 0

/*
 * 打印业务相关
 */
#define DEFAULT_REG_VAL 0

#define RASTER_SIM_PARAMS_REGS_NUM 2
#define PD_SIM_PARAMS_REGS_NUM 5
#define DIVISOR_PARAMS_REGS_NUM 3
#define JOB_PARAMS_REGS_NUM 5
#define PRINT_OFFSET_REGS_NUM 9
#define FIRE_DELAY_REGS_NUM 4
#define NOZZLE_SWITCH_REGS_NUM 6
#define SEARCH_LABEL_PARAMS_NUM 5


/* 打印核心管理信息 */
static print_info_t g_print_info;

static void delay_test(void)
{
	while (1)
		usleep(1000);
}

int main(void)
{	
	int ret;
	u32 block_size = TEST_BLOCK_SIZE;
	u32 block_cnt = TEST_BLOCK_CNT; 
	u8 *ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	u8 *ph1_data = ph0_data + PH1_USERBUF_OFFSET;	

	unsigned int dma_blks_one_time = 16;
	unsigned int sended_blks_cnt = 0;
	struct f2sm_read_info_t read_info;	
	int times;

	/* 设置打印管理结构体默认值 */
	init_print_info(&g_print_info);
	
	/* 初始化 */
	g_print_info.print_init(&g_print_info);	

	/* dma和prbs参数 */
	unsigned long ph0_down_dma_addr_arg = (unsigned long)g_print_info.mem_info[DOWN_MEM_INDEX].raw_addr;
	unsigned long ph1_down_dma_addr_arg = ph0_down_dma_addr_arg + PH1_USERBUF_OFFSET;
	unsigned long down_block_size_arg = dma_blks_one_time;
	unsigned long ph0_prb_seed_arg = 66;
	unsigned long ph1_prb_seed_arg = 99;


	/* 打印控制参数 */
	unsigned long config_master_arg = 0x1;
	unsigned long print_disable_arg = 0x0;
	unsigned long test_data_enable_arg = 0x1;
	unsigned long print_enable_arg = 0x1;
	unsigned long sync_sim_enable_arg = 0x1;

	/* 打印配置参数 */
	unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {1, 55};
	unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
	unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {3, 1, 8};
	unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {[0] = 0x0, [4] = 0x400000};	
	unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {DEFAULT_REG_VAL};
	unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
	unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
	unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_REG_VAL};	
	

	/* 构造发送给fpga的数据 */
	ret = build_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Build Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ret = check_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Check Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}	

	/* step 0 */
	ioctl(g_print_info.down_fd, DOWN_IOC_MASTER, &config_master_arg);

	/* step 1 */
	ioctl(g_print_info.down_fd, DOWN_IOC_PRINT_OPEN, &print_disable_arg);

	/* step 2 */
	ioctl(g_print_info.down_fd, DOWN_IOC_RASTER_SIM_PARAMS, &raster_sim_params_regs[0]);
	ioctl(g_print_info.down_fd, DOWN_IOC_PD_SIM_PARAMS, &pd_sim_params_regs[0]);

	/* step 3 */
	ioctl(g_print_info.down_fd, DOWN_IOC_DIVISOR_PARAMS, &divisor_params_regs[0]);
	ioctl(g_print_info.down_fd, DOWN_IOC_JOB_PARAMS, &job_params_regs[0]);
	ioctl(g_print_info.down_fd, DOWN_IOC_PRINT_OFFSET, &print_offset_regs[0]);
	ioctl(g_print_info.down_fd, DOWN_IOC_FIRE_DELAY, &fire_delay_regs[0]);
	ioctl(g_print_info.down_fd, DOWN_IOC_NOZZLE_SWITCH, &nozzle_switch_regs[0]);
	ioctl(g_print_info.down_fd, DOWN_IOC_SEARCH_LABEL_PARAMS, &search_label_params[0]);

	/* step 4 */
	ioctl(g_print_info.down_fd, DOWN_IOC_DATA_TEST, &test_data_enable_arg);

	/* step 5 */
	memset(&read_info,0,sizeof(read_info));
	ret = read(g_print_info.down_fd, &read_info, sizeof(read_info));
	printf("first read must not wait\n");
	if (ret != sizeof(read_info)) {
		printf("first read error\n");
		return -1;
	}
	
	ret = write(g_print_info.down_fd, ph0_data, dma_blks_one_time * block_size);
	if (ret != (int)(dma_blks_one_time * block_size)) {
		printf("first write error\n");
		return -1;		
	}

	//g_print_info.starttransfer_down_seed(&g_print_info, 66, 99, dma_blks_one_time);
	ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH0_DMA_ADDR, &ph0_down_dma_addr_arg);
	ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH1_DMA_ADDR, &ph1_down_dma_addr_arg);
	ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH0_PRB_SEED, &ph0_prb_seed_arg);
	ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH1_PRB_SEED, &ph1_prb_seed_arg);		
	ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_BLOCK_SIZE, &down_block_size_arg);
	ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_DMA_EN, NO_NEED_IOC_ARG);

	memset(&read_info, 0, sizeof(read_info));
	ret = read(g_print_info.down_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("second read error\n");
		return -1;
	}
	
	printf("first dma interrupt reav\n");
	sended_blks_cnt += dma_blks_one_time;
	

	ioctl(g_print_info.down_fd, DOWN_IOC_PRINT_OPEN, &print_enable_arg);
	ioctl(g_print_info.down_fd, DOWN_IOC_SYNC_SIM, &sync_sim_enable_arg);

	/* step 6 */

	/* 继续DMA发送，每次收到中断都启动下一次发送,直到数据全部发送完成则不响应中断. 
	 * 打印过程中持续检测硬件中断bit[4]和bit[5].
	 * 收到硬件中断bit[4]表示打印正常结束/收到硬件中断bit[5]表示打印异常停止.
	 * 打印输出寄存器内容,测试结束.
	 */
	for (times = 0; times < 16 * 1024; times++) {
		ret = write(g_print_info.down_fd, ph0_data, dma_blks_one_time * block_size);
		if (ret != (int)(dma_blks_one_time * block_size)) {
			printf("write error\n");
			return -1;
		}
		
		ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH0_DMA_ADDR, &ph0_down_dma_addr_arg);
		ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH1_DMA_ADDR, &ph1_down_dma_addr_arg);
		ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH0_PRB_SEED, &ph0_prb_seed_arg);
		ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_PH1_PRB_SEED, &ph1_prb_seed_arg);	
		ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_BLOCK_SIZE, &down_block_size_arg);
		ioctl(g_print_info.down_fd, DOWN_IOC_DOWN_DMA_EN, NO_NEED_IOC_ARG);
	
		memset(&read_info,0,sizeof(read_info));
		ret = read(g_print_info.down_fd, &read_info, sizeof(read_info));
		if (ret != sizeof(read_info)) {
			printf("get print interrupt or read error\n");
			goto finish_print;
		}	
	}

	/* 等待正常结束打印或者异常结束打印 */
	ret = read(g_print_info.down_fd, &read_info, sizeof(read_info));
finish_print:	
	if (ret == 1) 
		printf("down read normal stop\n");
	else if (ret ==2) 
		printf("down read accident stop\n");
	else 
		printf("down read wrong\n");
		

	/* step 7 */
	ioctl(g_print_info.down_fd, DOWN_IOC_PRINT_OPEN, &print_disable_arg);

	/* 释放资源 */
	free(ph0_data);
	g_print_info.print_close(&g_print_info);
	
	return ret;
}

