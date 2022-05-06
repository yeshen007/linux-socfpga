#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>


#include "print_config_control.h"
#include "utils.h"


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
static unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {3, 1, 8};
static unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {[0] = 0x0, [4] = 0x400000};
static unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_REG_VAL};

/* 打印核心管理信息 */
static print_info_t g_print_info;


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

	/* ioctl arg测试 */
	unsigned long o_test_data_en = 0x1;
	arg_info_t test_arg;
	test_arg.offset = O_TEST_DATA_EN;
	test_arg.size = ONE_REG_SIZE;
	test_arg.addr = (void*)&o_test_data_en;

	/* ioctl 复位测试 */
	arg_info_t reset_arg;
	reset_arg.offset = 0;
	reset_arg.size = 0;
	reset_arg.addr = 0;		

	/* 设置打印管理结构体默认值 */
	init_print_info(&g_print_info);

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

	/* 初始化 */
	g_print_info.print_init(&g_print_info);

	/* ioctl 复位测试 */
	//ioctl(g_print_info.down_fd, IOC_CMD_RESET, &reset_arg);

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
	//g_print_info.test_data_enable(&g_print_info);
	ioctl(g_print_info.down_fd, IOC_CMD_DOWN_WRITE, &test_arg);

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
	
	g_print_info.starttransfer_down_seed(&g_print_info, ph0_seed, ph1_seed, dma_blks_one_time);
	
	memset(&read_info, 0, sizeof(read_info));
	ret = read(g_print_info.down_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("second read error\n");
		return -1;
	}
	
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
	for (times = 0; times < 16 * 1024; times++) {
		ret = write(g_print_info.down_fd, ph0_data, dma_blks_one_time * block_size);
		if (ret != (int)(dma_blks_one_time * block_size)) {
			printf("write error\n");
			return -1;		
		}
		
		g_print_info.starttransfer_down_seed(&g_print_info, ph0_seed, ph1_seed, dma_blks_one_time);
		
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
	g_print_info.print_disable(&g_print_info);

	/* 释放资源 */
	free(ph0_data);
	g_print_info.print_close(&g_print_info);
	
	return 0;
}



