#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>



#include "print_config_control.h"



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



static int g_down_fd;


static unsigned long long block_size = TEST_BLOCK_SIZE;	//3kb
static unsigned long long down_blk_size = TEST_BLOCK_SIZE;	//3kb


static u8 *g_ph0_base; 
static u8 *g_ph1_base;
static u8 *g_ph0_change; 
static u8 *g_ph1_change;



static u8 linenum[16] = {15, 4, 9, 2, 12, 7, 10, 1, 14, 5, 8, 3, 13, 6, 11, 0};


static int build_base(void *ph0_base, void *ph1_base, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_base;
	u8 *buf_ph1 = (u8 *)ph1_base;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	
	/* build data */
	u8 row;
	u8 line;
	u32 block;
	for (block = 0; block < block_cnt; block++) {
		for (row = 0; row < 192; row++) {
			for (line = 0; line < 16; line++) {
				*buf_ph0++ = row; 
				*buf_ph1++ = 0;
			}
		}
	}
	
	return 0;

}

static int build_change(void *ph0_change, void *ph1_change, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_change;
	u8 *buf_ph1 = (u8 *)ph1_change;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	
	/* build data */
	u8 row;
	u8 line;
	u32 block;
	for (block = 0; block < block_cnt; block++) {
		for (row = 0; row < 192; row++) {
			for (line = 0; line < 16; line++) {
				*buf_ph0++ = 0; 
				*buf_ph1++ = (linenum[line] & 0xf) | (((u8)block << 4) & 0xf0);
			}
		}
	}
	
	return 0;

}


void base_call(void)
{
	int ret;
	struct f2sm_read_info_t read_info;	
	unsigned long long down_i;
	unsigned long long down_blks_one_time = 16;

	/* 基图数据量,一般不变,变的是change_total */
	unsigned long long base_total = TEST_BLOCK_SIZE * TEST_BLOCK_CNT;


	//发送第一块到最后一块
	for (down_i = 0; down_i < base_total / (down_blks_one_time * down_blk_size); down_i++) {
		ret = read(g_down_fd, &read_info, sizeof(read_info));
		if ((u32)ret != sizeof(read_info)) {	//为了和up保持代码一致，硬件不出问题不会在下行期间来结束打印中断
			if (ret == HAN_E_PT_FIN) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return ;
			} else if (ret == HAN_E_PT_EXPT) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return ;
			} else {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return ;
			}
		}	
		if (read_info.mem_index != 0) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return ;
		}

		ret = write(g_down_fd, g_ph0_base, down_blk_size * down_blks_one_time);
		if ((u32)ret != down_blk_size * down_blks_one_time) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return ;
		}
		g_print_info.starttransfer_down(&g_print_info, down_blks_one_time);
	}


}
	


/* 打印参数 */
unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {1, 55};
unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {0x1, DEFAULT_REG_VAL, 1, 100, 4096};
unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {24, 1, 8};
unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {[0] = 0x7, [3] = 4096, [4] = 4096*8};
unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {1200,(20<<16|0),(84<<16|64),(114<<16|94),(178<<16|158),
														  (206<<16|186),(270<<16|250),(300<<16|280),(364<<16|344)};
unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {100,1,4096,1,1};


int main(void)
{

	int ret;	
	unsigned long long dma_blks_one_time = 16;
	unsigned long long sended_blks_cnt = 0;
	struct f2sm_read_info_t read_info;
	unsigned long long times;

	/* 测量数据量 */
	unsigned long long change_total = TEST_BLOCK_SIZE * TEST_BLOCK_CNT * 64 * 2;
	

	g_ph0_base = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	g_ph1_base = g_ph0_base + PH1_USERBUF_OFFSET;
	g_ph0_change = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	g_ph1_change = g_ph0_change + PH1_USERBUF_OFFSET;


	ret = build_base(g_ph0_base, g_ph1_base, block_size, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Build Base Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ret = build_change(g_ph0_change, g_ph1_change, block_size, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Build Change Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}	

	/*  */
	Init_print_info(&g_print_info);

	
	g_down_fd = g_print_info.down_fd;	
	
	
	/* step 0 */
	g_print_info.config_master(&g_print_info);

	
	/* 
	 * step1
	 */
	g_print_info.print_disable(&g_print_info);
	
	/* 
	 * step2
	 */
	g_print_info.config_raster_sim_params(&g_print_info, raster_sim_params_regs);
	g_print_info.config_pd_sim_params(&g_print_info, pd_sim_params_regs);
	
	/*
	 * step3
	 */
	g_print_info.config_divisor_params(&g_print_info, divisor_params_regs);
	g_print_info.config_job_params(&g_print_info, job_params_regs);
	g_print_info.config_print_offset(&g_print_info, print_offset_regs);
	g_print_info.config_fire_delay(&g_print_info, fire_delay_regs);
	g_print_info.config_nozzle_switch(&g_print_info, nozzle_switch_regs);
	g_print_info.config_search_label_params(&g_print_info, search_label_params);	

	/*
	 * step4
	 */
	g_print_info.test_data_enable(&g_print_info);


	/* 
	 * step5
	 */	
	g_print_info.base_image_enable(&g_print_info);

	base_call();

	g_print_info.base_image_tranfer_complete(&g_print_info);


	g_print_info.base_image_disable(&g_print_info);

	
	 
	/* 启动DMA，发送第1块缓存数据。第一次收到DMA中断信号后，暂停发送数据 */
	memset(&read_info,0,sizeof(read_info));
	ret = read(g_down_fd, &read_info, sizeof(read_info));
	printf("first read must not wait\n");
	if (ret != sizeof(read_info)) {
		printf("first read error\n");
		return -1;
	}
	
	ret = write(g_down_fd, g_ph0_change, dma_blks_one_time * down_blk_size);
	if (ret != (int)(dma_blks_one_time * down_blk_size)) {
		printf("first write error\n");
		return -1;		
	}
	
	g_print_info.starttransfer_down(&g_print_info, dma_blks_one_time);
	
	memset(&read_info,0,sizeof(read_info));
	ret = read(g_down_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("second read error\n");
		return -1;
	}
	
	printf("first dma interrupt reav\n");
	sended_blks_cnt += dma_blks_one_time;


	/* 启动打印 */
	g_print_info.print_enable(&g_print_info);

	/* 开始硬件仿真 */
	g_print_info.sync_sim_enable(&g_print_info);


	/* 
	 * step6
	 */		

	/* 继续DMA发送，每次收到中断都启动下一次发送，直到数据全部发送完成则不响应中断. 
     * 打印过程中持续检测硬件中断bit[4]和bit[5].
     * 收到硬件中断bit[4]表示打印正常结束/收到硬件中断bit[5]表示打印异常停止.
     * 打印输出寄存器内容,测试结束.
	 */
	for (times = 1; times < change_total / (dma_blks_one_time * down_blk_size); times++) {
		ret = write(g_down_fd, g_ph0_change, dma_blks_one_time * down_blk_size);
		if (ret != (int)(dma_blks_one_time * down_blk_size)) {
			printf("write error\n");
			return -1;		
		}
		
		g_print_info.starttransfer_down(&g_print_info, dma_blks_one_time);
		
		memset(&read_info,0,sizeof(read_info));
		ret = read(g_down_fd, &read_info, sizeof(read_info));
		if (ret != sizeof(read_info)) {
			printf("get print interrupt or read error\n");
			goto finish_print;
		}	

	}

	/* 等待正常结束打印或者异常结束打印 */
	ret = read(g_down_fd, &read_info, sizeof(read_info));
finish_print:	
	if (ret == HAN_E_PT_FIN) 
		printf("down read normal stop\n");
	else if (ret == HAN_E_PT_EXPT) 
		printf("down read accident stop\n");
	else 
		printf("down read wrong,ret value %d\n", ret);	
	 

	/* 
	 * step7
	 */	
	//g_print_info.print_disable(&g_print_info);
	

	/* 释放数据 */
	free(g_ph0_base);
	free(g_ph0_change);

	Close_print_info(&g_print_info);

	return ret;
}





