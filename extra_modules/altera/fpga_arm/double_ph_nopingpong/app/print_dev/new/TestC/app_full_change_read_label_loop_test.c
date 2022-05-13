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


static int up_fd;
static int down_fd;

static u32 down_blk_size = TEST_BLOCK_SIZE;
static u32 up_blk_size = TEST_BLOCK_SIZE / 3; //1kb

static u8 *ph0_data; 
static u8 *ph1_data;
static u8 *up_buf; 

static int start_print = 0;

void *down_thread(void *arg);


/* 测试数据量,调节o_job_fire_num然后算出m,再算出total_times */
#define TEST_JOB_FIRE_NUM 10*1000
#define TEST_JOB_NUM 16
unsigned int o_job_fire_num = TEST_JOB_FIRE_NUM;
unsigned int job_num = TEST_JOB_NUM;

/* 打印参数 */
unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {1, 1500};
unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {0x1, DEFAULT_REG_VAL, TEST_JOB_NUM, 100, TEST_JOB_FIRE_NUM};
unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {24, 1, 8};
unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {[0] = 0x5, [3] = TEST_JOB_FIRE_NUM, [4] = 0x400000};
unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {1200,(20<<16|0),(84<<16|64),(114<<16|94),(178<<16|158),
														  (206<<16|186),(270<<16|250),(300<<16|280),(364<<16|344)};
unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {100,1,TEST_JOB_FIRE_NUM,1,1};	


int main(void)
{	
	int ret;
	u32 up_i;	//收到的第几个1kb	
	void *down_ret;
	pthread_t down_tid;
	
	u32 up_blks_one_time = 16;
	up_buf = (u8 *)malloc(2 * PH1_USERBUF_OFFSET); 
	ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	ph1_data = ph0_data + PH1_USERBUF_OFFSET;
	

	/* 构造发送给fpga的数据 */
	ret = build_fpga_data(ph0_data, ph1_data, TEST_BLOCK_SIZE, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Build Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ret = check_fpga_data(ph0_data, ph1_data, TEST_BLOCK_SIZE, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Check Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}	

	/* 初始化,这两步缺一不可
	 * 先通过init_print_info设置g_print_info的函数和数据成员的默认值
     * 然后在通过设置好的print_init成员函数打开设备文件,设置好g_print_info的数据成员
	 */
	Init_print_info(&g_print_info);

	
	up_fd = g_print_info.up_fd;
	down_fd = g_print_info.down_fd;
	
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

	g_print_info.loop_test_enable(&g_print_info);

	/* 启动一次上行传输 */
	g_print_info.starttransfer_up(&g_print_info, up_blks_one_time);

	/* 
	 * step5
	 */
	/* 下行子线程 */
	ret = pthread_create(&down_tid, NULL, down_thread, NULL); 
	if (ret != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_free_mem;
	}

	while (start_print != 1) 
		usleep(5000);


	/* 开始打印 */	
	g_print_info.print_enable(&g_print_info);
	printf("start print\n");

	/* 开始硬件仿真 */
	g_print_info.sync_sim_enable(&g_print_info);
	

	/* 
	 * step6
	 */

	/* 打印过程持续检测上行DMA中断。每次收到中断则启动下次一数据传输(配置数据块大小,地址6写1写0)
	 * 此过程软件不需要主动结束，有中断上来就一直配置传输
	 * 打印过程中持续检测硬件中断bit[5]和bit[4]。收到硬件中断bit[4]表示打印正常结束/收到硬件
	 * 中断bit[5]表示打印异常停止。打印输出寄存器内容,测试结束
	 */ 
	for (up_i = 0; ; up_i++) {
		ret = read(up_fd, up_buf, up_blk_size * up_blks_one_time);	
		if ((u32)ret != up_blk_size * up_blks_one_time) {			//打印结束中断
			if (ret == HAN_E_PT_FIN) {
				printf("up read normal stop, index %d\n", up_i);			
				goto loop_release_down_thread;
			} else if (ret == HAN_E_PT_EXPT) {
				printf("up read accident stop, index %d\n", up_i);
				goto loop_release_down_thread;
			} else {
				printf("up read wrong, index %d\n", up_i);
				goto loop_release_down_thread;
			}
		}
		g_print_info.starttransfer_up(&g_print_info, up_blks_one_time);
	}

	
loop_release_down_thread:
	ret = pthread_join(down_tid, &down_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (down_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);

loop_release_free_mem:
	

	/* step 7 */
	//g_print_info.print_disable(&g_print_info);

	/* 释放资源 */
	free(up_buf);
	free(ph0_data);
	Close_print_info(&g_print_info);
	
	return 0;
}



void *down_thread(void *arg)
{
	int ret;
	struct f2sm_read_info_t read_info;	
	u32 down_i;
	u32 down_blks_one_time = 16;
	u32 m = (o_job_fire_num * job_num)/4;				
	u32 total_times = m/down_blks_one_time;


	//先发送一块dma
	ret = read(down_fd, &read_info, sizeof(read_info));
	if ((u32)ret != sizeof(read_info) || read_info.mem_index != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	ret = write(down_fd, ph0_data, down_blk_size * down_blks_one_time);
	if ((u32)ret != down_blk_size * down_blks_one_time) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	g_print_info.starttransfer_down(&g_print_info, down_blks_one_time);

	start_print = 1;


	//发送第二块到最后一块
	for (down_i = 1; down_i < total_times; down_i++) {
		ret = read(down_fd, &read_info, sizeof(read_info));
		if ((u32)ret != sizeof(read_info)) {	//为了和up保持代码一致，硬件不出问题不会在下行期间来结束打印中断
			if (ret == HAN_E_PT_FIN) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return NULL;
			} else if (ret == HAN_E_PT_EXPT) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return NULL;
			} else {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return NULL;
			}
		}	
		if (read_info.mem_index != 0) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}

		ret = write(down_fd, ph0_data, down_blk_size * down_blks_one_time);
		if ((u32)ret != down_blk_size * down_blks_one_time) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}
		g_print_info.starttransfer_down(&g_print_info, down_blks_one_time);
	}


	return (void *)1;
}



