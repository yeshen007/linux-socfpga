#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <sys/stat.h>
#include <sched.h>

#include "print_config_control.h"
#include "utils.h"

#define RASTER_SIM_PARAMS_REGS_NUM 2
#define PD_SIM_PARAMS_REGS_NUM 5
#define DIVISOR_PARAMS_REGS_NUM 3
#define JOB_PARAMS_REGS_NUM 5
#define PRINT_OFFSET_REGS_NUM 9
#define FIRE_DELAY_REGS_NUM 4
#define NOZZLE_SWITCH_REGS_NUM 6
#define SEARCH_LABEL_PARAMS_NUM 5

#define DEFAULT_REG_VAL 0

/* 打印模式 */
#define FULL_CHANGE_NO_READ_LABEL 				
//#define FULL_CHANGE_READ_LABEL					
//#define FULL_CHANGE_READ_LABEL_LOOPTEST			
//#define BASE_IMAGE_CHANGE_READ_LABEL_LOOPTEST	

/* 打印参数 */
#ifdef 	FULL_CHANGE_NO_READ_LABEL
static unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};	
static unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_REG_VAL};
#endif
#ifdef 	FULL_CHANGE_READ_LABEL
static unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};	
static unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_REG_VAL};
#endif
#ifdef 	FULL_CHANGE_READ_LABEL_LOOPTEST
static unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {DEFAULT_REG_VAL};	
static unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_REG_VAL};
static unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_REG_VAL};
#endif
#ifdef 	BASE_IMAGE_CHANGE_READ_LABEL_LOOPTEST
static unsigned long raster_sim_params_regs[RASTER_SIM_PARAMS_REGS_NUM] = {DEFAULT_VAL};
static unsigned long pd_sim_params_regs[PD_SIM_PARAMS_REGS_NUM] = {DEFAULT_VAL};
static unsigned long divisor_params_regs[DIVISOR_PARAMS_REGS_NUM] = {DEFAULT_VAL};
static unsigned long job_params_regs[JOB_PARAMS_REGS_NUM] = {DEFAULT_VAL};	
static unsigned long print_offset_regs[PRINT_OFFSET_REGS_NUM] = {DEFAULT_VAL};
static unsigned long fire_delay_regs[FIRE_DELAY_REGS_NUM] = {DEFAULT_VAL};
static unsigned long nozzle_switch_regs[NOZZLE_SWITCH_REGS_NUM] = {DEFAULT_VAL};
static unsigned long search_label_params[SEARCH_LABEL_PARAMS_NUM] = {DEFAULT_VAL};
#endif

/* 打印核心管理信息 */
static print_info_t g_print_info;


int main(void)
{	
	int ret;
	u32 block_size = TEST_BLOCK_SIZE;
	u32 block_cnt = TEST_BLOCK_CNT;	
	u8 *ph0_data = (u8 *)malloc(2*PH2_USERBUF_OFFSET);
	u8 *ph1_data = ph0_data + PH2_USERBUF_OFFSET;	

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
	//TODO

	/* step 5 */
	//TODO

	/* step 6 */
	//TODO

	/* step 7 */
	//TODO

	/* 释放资源 */
	g_print_info.print_close(&g_print_info);
	
	return 0;
}
