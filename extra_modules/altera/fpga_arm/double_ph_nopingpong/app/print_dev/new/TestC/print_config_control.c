#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>


#include "print_config_control.h"


static void _print_init(struct print_info *print_info)
{
	unsigned long dma_info[4];
	arg_info_t arg_info;	

	print_info->up_fd = open(UP_DEV, O_RDWR);
    if (print_info->up_fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }	
	
	print_info->down_fd = open(DOWN_DEV, O_RDWR);
    if (print_info->down_fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
		close(print_info->up_fd);
        exit(-1);
    }	

	arg_info.offset = NO_NEED_OFFSET;
	arg_info.size = 4*ONE_REG_SIZE;
	arg_info.addr = (void*)&dma_info;
	Ioctl(print_info->down_fd, IOC_CMD_PH_DMA, &arg_info);

    print_info->mem_info[UP_MEM_INDEX].raw_addr = (void *)dma_info[0];
    print_info->mem_info[UP_MEM_INDEX].mem_size = dma_info[1];

    print_info->mem_info[DOWN_MEM_INDEX].raw_addr = (void *)dma_info[2];
    print_info->mem_info[DOWN_MEM_INDEX].mem_size = dma_info[3];	

}

static void _print_close(struct print_info *print_info)
{
	int i;
	for (i = 0; i < F2SM_MEM_NUMS; i++)
		memset(&print_info->mem_info[i], 0, sizeof(f2sm_mem_info_t));
	
	close(print_info->down_fd);
	print_info->down_fd = -1;
	
	close(print_info->up_fd);
	print_info->up_fd = -1;	
}



static void _starttransfer_down_seed(struct print_info *print_info, unsigned long seed1, 
												unsigned long seed2, unsigned long blk_count)   
{
	unsigned long reg_val;
	arg_info_t arg_info;
	
	reg_val = (unsigned long)print_info->mem_info[DOWN_MEM_INDEX].raw_addr;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	reg_val = (unsigned long)print_info->mem_info[DOWN_MEM_INDEX].raw_addr + PH1_OFFSET;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
	
	reg_val = seed1;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH0;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	reg_val = seed2;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH1;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info); 

	reg_val = blk_count;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_EN;
	arg_info.size = NO_NEED_ARG_SIZE;
	arg_info.addr = NO_NEED_ARG_ADDR;	
	Ioctl(print_info->down_fd, IOC_CMD_NONE, &arg_info);	
}

static void _starttransfer_down(struct print_info *print_info, unsigned int blk_count)   
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = (unsigned long)print_info->mem_info[DOWN_MEM_INDEX].raw_addr;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	

	reg_val = (unsigned long)print_info->mem_info[DOWN_MEM_INDEX].raw_addr + PH1_OFFSET;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	reg_val = blk_count;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_EN;
	arg_info.size = NO_NEED_ARG_SIZE;
	arg_info.addr = NO_NEED_ARG_ADDR;	
	Ioctl(print_info->down_fd, IOC_CMD_NONE, &arg_info);	
}

static void _starttransfer_up(struct print_info *print_info, unsigned int blk_count)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = (unsigned long)print_info->mem_info[UP_MEM_INDEX].raw_addr;
	arg_info.offset = PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH0;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	

	reg_val = (unsigned long)print_info->mem_info[UP_MEM_INDEX].raw_addr + PH1_OFFSET;
	arg_info.offset = PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH1;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	reg_val = blk_count;
	arg_info.offset = PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BLOCK_SIZE;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	arg_info.offset = PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN;
	arg_info.size = NO_NEED_ARG_SIZE;
	arg_info.addr = NO_NEED_ARG_ADDR;
	Ioctl(print_info->down_fd, IOC_CMD_NONE, &arg_info);

}


static void _print_enable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x1;
	arg_info.offset = PRINT_OPEN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}

static void _print_disable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x0;
	arg_info.offset = PRINT_OPEN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}

static void _config_master(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x1;
	arg_info.offset = O_MASTER_INDICATOR;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}

static void _config_slave(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x0;
	arg_info.offset = O_MASTER_INDICATOR;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}

static void _sync_sim_enable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x1;
	arg_info.offset = MASTER_SYNC_SIM_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}


static void _test_data_enable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x1;
	arg_info.offset = O_TEST_DATA_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);		
}

static void _loop_test_enable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x1;
	arg_info.offset = O_LOOP_TEST_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);		
}

static void _base_image_enable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x1;
	arg_info.offset = O_BASE_IMAGE_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}


static void _base_image_disable(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	reg_val = 0x0;
	arg_info.offset = O_BASE_IMAGE_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}

static void _base_image_tranfer_complete(struct print_info *print_info)
{
	unsigned long reg_val;
	arg_info_t arg_info;

	arg_info.offset = I_BASE_IMAGE_TRANFER_COMPLETE;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;		
	do {
		Ioctl(print_info->down_fd, IOC_CMD_READ, &arg_info);
	} while (reg_val != 0x1);
}


static void _config_raster_sim_params(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[2];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	arg_info.offset = O_MASTER_RASTER_SEL;
	arg_info.size = 2*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}

static void _config_pd_sim_params(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[5];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];
	reg_val[3] = regs[3];
	reg_val[4] = regs[4];	
	arg_info.offset = O_MASTER_PD_SEL;
	arg_info.size = 5*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}


static void _config_divisor_params(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[3];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];	
	arg_info.offset = O_MASTER_DPI_D_FACTOR;
	arg_info.size = 3*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}

static void _config_job_params(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[5];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];
	reg_val[3] = regs[3];
	reg_val[4] = regs[4];	
	arg_info.offset = O_WORK_MODE;
	arg_info.size = 5*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}

static void _config_print_offset(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[9];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];
	reg_val[3] = regs[3];
	reg_val[4] = regs[4];	
	reg_val[5] = regs[5];
	reg_val[6] = regs[6];
	reg_val[7] = regs[7];
	reg_val[8] = regs[8];	
	arg_info.offset = O_PH_OFFSET;
	arg_info.size = 9*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}

static void _config_fire_delay(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[4];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];
	reg_val[3] = regs[3];	
	arg_info.offset = O_DPI_DELAY_NUM_PH0;
	arg_info.size = 4*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);		
}


static void _config_nozzle_switch(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[6];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];
	reg_val[3] = regs[3];
	reg_val[4] = regs[4];
	reg_val[5] = regs[5];
	
	arg_info.offset = O_SW_INIT_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	usleep(10000);

	arg_info.offset = O_SW_RW_CPU;
	arg_info.size = 5*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[1];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);
}

static void _config_search_label_params(struct print_info *print_info, unsigned long *regs)
{
	unsigned long reg_val[5];
	arg_info_t arg_info;

	reg_val[0] = regs[0];
	reg_val[1] = regs[1];
	reg_val[2] = regs[2];
	reg_val[3] = regs[3];
	reg_val[4] = regs[4];

	arg_info.offset = O_PD_CHK_THRESHOLD;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[0];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	

	arg_info.offset = O_PD_SERACH_MODE;
	arg_info.size = 4*ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val[1];
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
}


void init_print_info(struct print_info *print_info)
{	
	int i;

	print_info->up_fd = -1;
	print_info->down_fd = -1;
	for (i = 0; i < F2SM_MEM_NUMS; i++)
		memset(&print_info->mem_info[i], 0, sizeof(f2sm_mem_info_t));
	
	print_info->print_init = _print_init;
	print_info->print_close = _print_close;
	print_info->starttransfer_down_seed = _starttransfer_down_seed;
	print_info->starttransfer_down = _starttransfer_down;
	print_info->starttransfer_up = _starttransfer_up;
	
	print_info->print_enable = _print_enable;
	print_info->print_disable = _print_disable;
	print_info->config_master = _config_master;
	print_info->config_slave = _config_slave;
	print_info->sync_sim_enable = _sync_sim_enable;
	print_info->test_data_enable = _test_data_enable;
	print_info->loop_test_enable = _loop_test_enable;
	print_info->base_image_enable = _base_image_enable;
	print_info->base_image_disable = _base_image_disable;
	print_info->base_image_tranfer_complete = _base_image_tranfer_complete;
	print_info->config_raster_sim_params = _config_raster_sim_params;
	print_info->config_pd_sim_params = _config_pd_sim_params;
	print_info->config_divisor_params = _config_divisor_params;
	print_info->config_job_params = _config_job_params;
	print_info->config_print_offset = _config_print_offset;
	print_info->config_fire_delay = _config_fire_delay;
	print_info->config_nozzle_switch = _config_nozzle_switch;
	print_info->config_search_label_params = _config_search_label_params;
}


void close_print_info(struct print_info *print_info)
{
	print_info->print_init = NULL;
	print_info->print_close = NULL;
	print_info->starttransfer_down_seed = NULL;
	print_info->starttransfer_down = NULL;
	print_info->starttransfer_up = NULL;
	
	print_info->print_enable = NULL;
	print_info->print_disable = NULL;
	print_info->config_master = NULL;
	print_info->config_slave = NULL;
	print_info->sync_sim_enable = NULL;
	print_info->test_data_enable = NULL;
	print_info->loop_test_enable = NULL;
	print_info->base_image_enable = NULL;
	print_info->base_image_disable = NULL;
	print_info->base_image_tranfer_complete = NULL;
	print_info->config_raster_sim_params = NULL;
	print_info->config_pd_sim_params = NULL;
	print_info->config_divisor_params = NULL;
	print_info->config_job_params = NULL;
	print_info->config_print_offset = NULL;
	print_info->config_fire_delay = NULL;
	print_info->config_nozzle_switch = NULL;
	print_info->config_search_label_params = NULL;
}


void Init_print_info(struct print_info *print_info)
{
	init_print_info(print_info);
	print_info->print_init(print_info);
}


void Close_print_info(struct print_info *print_info)
{
	print_info->print_close(print_info);
	close_print_info(print_info);
}

