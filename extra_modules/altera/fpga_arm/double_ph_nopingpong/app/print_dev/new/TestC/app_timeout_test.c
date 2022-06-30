#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>


#include "print_config_control.h"


/* 测试数据量 */
#define TEST_TIMES 	(10*1024)


#define DEFAULT_REG_VAL 0

#define RASTER_SIM_PARAMS_REGS_NUM 2
#define PD_SIM_PARAMS_REGS_NUM 5
#define DIVISOR_PARAMS_REGS_NUM 3
#define JOB_PARAMS_REGS_NUM 5
#define PRINT_OFFSET_REGS_NUM 9
#define FIRE_DELAY_REGS_NUM 4
#define NOZZLE_SWITCH_REGS_NUM 6
#define SEARCH_LABEL_PARAMS_NUM 5

/* 打印参数 */
static unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {1, 55};
static unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {24, 1, 8};
static unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {[0] = 0x1, [4] = 0x400000};
static unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {1200,(20<<16|0),(84<<16|64),(114<<16|94),(178<<16|158),
														  (206<<16|186),(270<<16|250),(300<<16|280),(364<<16|344)};

static unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_REG_VAL};

/* 打印核心管理信息 */
static print_info_t g_print_info;

/* poll测试 */
#define POLL_TEST

int main(void)
{	
	int ret;
	u32 block_size = TEST_BLOCK_SIZE;
	u32 block_cnt = TEST_BLOCK_CNT;	
	u8 *ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	u8 *ph1_data = ph0_data + PH1_USERBUF_OFFSET;	

	int times;
	struct f2sm_read_info_t read_info;
	unsigned int dma_blks_one_time = 16;
	unsigned int sended_blks_cnt = 0;
	unsigned long ph0_seed = 66;
	unsigned long ph1_seed = 99;

#ifdef POLL_TEST
	/* poll测试参数 */
	struct pollfd fds;
	nfds_t nfds = 1;
#endif	

	/* 构造发送给fpga的数据 */
	build_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	check_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	

	/* 初始化
	 * 先通过init_print_info设置g_print_info的函数和数据成员的默认值
     * 然后在通过设置好的print_init成员函数打开设备文件,设置好g_print_info的数据成员
	 */
	init_print_info(&g_print_info);

	/* step 0 */
	g_print_info.config_master(&g_print_info);

	/* step 1 */
	g_print_info.print_disable(&g_print_info);

	/* step 2 */
	g_print_info.config_raster_sim_params(&g_print_info, raster_sim_params_regs);
	g_print_info.config_pd_sim_params(&g_print_info, pd_sim_params_regs);

	/* step 3 */
	g_print_info.config_divisor_params(&g_print_info, divisor_params_regs);
	g_print_info.config_job_params(&g_print_info, job_params_regs);
	g_print_info.config_print_offset(&g_print_info, print_offset_regs);
	g_print_info.config_fire_delay(&g_print_info, fire_delay_regs);
	g_print_info.config_nozzle_switch(&g_print_info, nozzle_switch_regs);
	g_print_info.config_search_label_params(&g_print_info, search_label_params);

	/* step 4 */
	g_print_info.test_data_enable(&g_print_info);

	/* step 5 */
	Read(g_print_info.down_fd, &read_info, sizeof(read_info));
	
	printf("first read must not wait\n");

	Write(g_print_info.down_fd, ph0_data, dma_blks_one_time * block_size);
	
	g_print_info.starttransfer_down_seed(&g_print_info, ph0_seed, ph1_seed, dma_blks_one_time);

#ifdef POLL_TEST	
	memset(&fds,0,sizeof(fds));
	fds.fd = g_print_info.down_fd;
	fds.events |= POLLIN;
	Poll(&fds, nfds, 10);
	if (!(fds.revents & POLLIN)) 
		printf("first dma interrupt poll timeout\n");
#endif

	Read(g_print_info.down_fd, &read_info, sizeof(read_info));
	
	printf("first dma interrupt reav\n");
	
	sended_blks_cnt += dma_blks_one_time;

	g_print_info.print_enable(&g_print_info);	
	g_print_info.sync_sim_enable(&g_print_info);

	/* step 6 */

	/* 继续DMA发送，每次收到中断都启动下一次发送，直到数据全部发送完成则不响应中断. 
	 * 打印过程中持续检测硬件中断bit[4]和bit[5].
	 * 收到硬件中断bit[4]表示打印正常结束/收到硬件中断bit[5]表示打印异常停止.
	 * 打印输出寄存器内容,测试结束.
	 */
	for (times = 1; times < TEST_TIMES; times++) {
		Write(g_print_info.down_fd, ph0_data, dma_blks_one_time * block_size);
		
		g_print_info.starttransfer_down_seed(&g_print_info, ph0_seed, ph1_seed, dma_blks_one_time);

#ifdef POLL_TEST
		memset(&fds,0,sizeof(fds));
		fds.fd = g_print_info.down_fd;
		fds.events |= POLLIN;		
		Poll(&fds, nfds, 10);
		if (!(fds.revents & POLLIN)) 
			printf("times %d dma interrupt poll timeout\n", times);				
#endif
		ret = Read(g_print_info.down_fd, &read_info, sizeof(read_info));
		if (ret != sizeof(read_info)) 
			goto finish_print;	
	}

	/* 等待正常结束打印或者异常结束打印 */
	ret = Read(g_print_info.down_fd, &read_info, sizeof(read_info));
finish_print:	
	if (ret == HAN_E_PT_FIN) 
		printf("down read normal stop, ok\n");
	else 
		printf("down read stop bad at or before the last time\n");
			

	/* 释放资源 */
	close_print_info(&g_print_info);
	free(ph0_data);
	
	return 0;
}



