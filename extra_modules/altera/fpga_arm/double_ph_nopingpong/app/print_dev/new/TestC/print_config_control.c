#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "print_config_control.h"


static void _print_init(struct print_info *print_info)
{
	unsigned long dma_info[4];
	arg_info_t arg_info;	

	print_info->up_fd = Open(UP_DEV, O_RDWR);
    if (print_info->up_fd < 0) {
        fprintf(stderr, "Open: %s\n", strerror(errno));
        exit(-1);
    }
	
	print_info->down_fd = Open(DOWN_DEV, O_RDWR);
    if (print_info->down_fd < 0) {
        fprintf(stderr, "Open: %s\n", strerror(errno));
		Close(print_info->up_fd);
        exit(-1);
    }	

	arg_info.offset = NO_NEED_ARG_OFFSET;
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
	
	Close(print_info->down_fd);
	print_info->down_fd = -1;
	
	Close(print_info->up_fd);
	print_info->up_fd = -1;	
}

static void _print_stop(struct print_info *print_info)
{
	Ioctl(print_info->down_fd, IOC_CMD_STOP, NULL);	
}

static void _print_reset(struct print_info *print_info)
{
	Ioctl(print_info->down_fd, IOC_CMD_RESET, NULL);	
}

static void _print_nonblock_up(struct print_info *print_info)
{
	int flags;

	flags = fcntl(print_info->up_fd, F_GETFL, 0);
	if (flags < 0) {
		fprintf(stderr, "Nonblock up get flags faild: %s\n", strerror(errno));
		exit(-1);
	}

	flags |= O_NONBLOCK;

	if (fcntl(print_info->up_fd, F_SETFL, flags) < 0) {
		fprintf(stderr, "Nonblock up set flags faild: %s\n", strerror(errno));
		exit(-1);
	}		
}

static void _print_nonblock_down(struct print_info *print_info)
{
	int flags;

	flags = fcntl(print_info->down_fd, F_GETFL, 0);
	if (flags < 0) {
		fprintf(stderr, "Nonblock down get flags faild: %s\n", strerror(errno));
		exit(-1);
	}

	flags |= O_NONBLOCK;

	if (fcntl(print_info->down_fd, F_SETFL, flags) < 0) {
		fprintf(stderr, "Nonblock down set flags faild: %s\n", strerror(errno));
		exit(-1);
	}		
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

	reg_val = 0x1;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	

	reg_val = 0x0;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
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

	reg_val = 0x1;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	

	reg_val = 0x0;
	arg_info.offset = PRINTER_DATA_MASTER_READ_CONTROL_READ_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;	
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);	
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

	reg_val = 0x1;
	arg_info.offset = PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

	reg_val = 0x0;
	arg_info.offset = PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;
	Ioctl(print_info->down_fd, IOC_CMD_WRITE, &arg_info);

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

static void _get_board_id(struct print_info *print_info, unsigned long *id)
{
	arg_info_t arg_info;

	arg_info.offset = I_TX_ID_I_RX_ID_I_BOARD_ID;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)id;		
	Ioctl(print_info->down_fd, IOC_CMD_READ, &arg_info);
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


static void _debug_output_regs(struct print_info *print_info)
{
	unsigned long reg_index;
	unsigned long reg_val;
	arg_info_t arg_info;
	arg_info.size = ONE_REG_SIZE;
	arg_info.addr = (void*)&reg_val;		

	FILE *regs_value_file;
	regs_value_file = fopen("regs.txt", "w+");
	if (regs_value_file == NULL)
		fprintf(stderr, "open regs.txt error\n");
	
	/* 将寄存器值写入regs.txt */
	for (reg_index = 0; reg_index <= 355; reg_index++) {
		arg_info.offset = reg_index;
		Ioctl(print_info->down_fd, IOC_CMD_READ, &arg_info);
		fprintf(regs_value_file, "reg %lu -- decvalue %ld -- hexvalue 0x%08lx\n", 
				reg_index, (long)reg_val, reg_val);
	}

	fclose(regs_value_file);
}



static void _init_print_info(struct print_info *print_info)
{		
	print_info->print_init = _print_init;
	print_info->print_close = _print_close;
	print_info->print_stop = _print_stop;
	print_info->print_reset = _print_reset;
	print_info->print_nonblock_up = _print_nonblock_up;
	print_info->print_nonblock_down = _print_nonblock_down;
	
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
	print_info->get_borad_id = _get_board_id;
	
	print_info->config_raster_sim_params = _config_raster_sim_params;
	print_info->config_pd_sim_params = _config_pd_sim_params;
	print_info->config_divisor_params = _config_divisor_params;
	print_info->config_job_params = _config_job_params;
	print_info->config_print_offset = _config_print_offset;
	print_info->config_fire_delay = _config_fire_delay;
	print_info->config_nozzle_switch = _config_nozzle_switch;
	print_info->config_search_label_params = _config_search_label_params;

	print_info->debug_output_regs = _debug_output_regs;
}


static void _close_print_info(struct print_info *print_info)
{
	print_info->print_init = NULL;
	print_info->print_close = NULL;
	print_info->print_stop = NULL;
	print_info->print_reset = NULL;
	print_info->print_nonblock_up = NULL;
	print_info->print_nonblock_down = NULL;	
	
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
	print_info->get_borad_id = NULL;
	
	print_info->config_raster_sim_params = NULL;
	print_info->config_pd_sim_params = NULL;
	print_info->config_divisor_params = NULL;
	print_info->config_job_params = NULL;
	print_info->config_print_offset = NULL;
	print_info->config_fire_delay = NULL;
	print_info->config_nozzle_switch = NULL;
	print_info->config_search_label_params = NULL;

	print_info->debug_output_regs = NULL;
}


void init_print_info(struct print_info *print_info)
{
	_init_print_info(print_info);
	print_info->print_init(print_info);
}


void close_print_info(struct print_info *print_info)
{
	print_info->print_close(print_info);
	_close_print_info(print_info);
}

