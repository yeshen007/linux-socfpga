#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "print_config_control.h"
#include "fpga_regs.h"
#include "alt_f2sm_regs.h"


static void _print_init(struct print_info *print_info)
{

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

	print_info->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (print_info->mem_fd == -1) {
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(print_info->down_fd);
		close(print_info->up_fd);
		exit(-1);
	}
	
	print_info->virtual_base = mmap(NULL, MMAP_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, print_info->mem_fd, MMAP_BASE);
    if (print_info->virtual_base == MAP_FAILED) {
        close(print_info->mem_fd);
        close(print_info->down_fd);
		close(print_info->up_fd);
        exit(-1);
    }	

    print_info->mem_info[UP_MEM_INDEX].raw_addr = (void *)MEM_PHY_UP;
    print_info->mem_info[UP_MEM_INDEX].mem_size = MEM_PHY_UP_SIZE;

    print_info->mem_info[DOWN_MEM_INDEX].raw_addr = (void *)MEM_PHY_DOWN;
    print_info->mem_info[DOWN_MEM_INDEX].mem_size = MEM_PHY_DOWN_SIZE;	

}

static void _print_close(struct print_info *print_info)
{
	int i;
	for (i = 0; i < F2SM_MEM_NUMS; i++)
		memset(&print_info->mem_info[i], 0, sizeof(f2sm_mem_info_t));

	if (print_info->virtual_base) {
        if (munmap(print_info->virtual_base, MMAP_SPAN) != 0) {
            printf("ERROR: munmap() failed...\n");
			if (print_info->mem_fd != -1) {
				close(print_info->mem_fd);
				print_info->mem_fd = -1;
			}
            if (print_info->down_fd != -1) {
                close(print_info->down_fd);
           		print_info->down_fd = -1;
            }
            if (print_info->up_fd != -1) {
                close(print_info->up_fd);
            	print_info->up_fd = -1;
            }
            return;
        }
		print_info->virtual_base = NULL;
	}
	
	if (print_info->mem_fd) {
		close(print_info->mem_fd);
		print_info->mem_fd = -1;
	}

	if (print_info->down_fd) {
		close(print_info->down_fd);
		print_info->down_fd = -1;
	}	

	if (print_info->up_fd) {
		close(print_info->up_fd);
		print_info->up_fd = -1;
	}	

}



static void _starttransfer_down_seed(struct print_info *print_info, unsigned long seed1, 
												unsigned long seed2, unsigned long blk_count)   
{
    void * reg_addr =  NULL;

	/* 将ph0和ph1起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)print_info->mem_info[DOWN_MEM_INDEX].raw_addr));
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)print_info->mem_info[DOWN_MEM_INDEX].raw_addr + PH1_OFFSET));

	
	/* 将ph0种子和ph1种子写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH0);
    alt_write_word(reg_addr, seed1);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH1);
    alt_write_word(reg_addr, seed2);

	
	/* 将ph0和ph1地址空间大小写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE);
    alt_write_word(reg_addr, blk_count & 0xFFFF);
	
	/* 启动ph0和ph1的fpga读 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_EN);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
}

static void _starttransfer_down(struct print_info *print_info, int down_mem_index, unsigned int blk_count)   
{
    void * reg_addr =  NULL;

	/* 将ph0起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)print_info->mem_info[down_mem_index].raw_addr));

	/* 将ph1起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)print_info->mem_info[down_mem_index].raw_addr + PH1_OFFSET));

	/* 将ph0和ph1地址空间大小写入fpga寄存器，单位3kb */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* 启动fpga读 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_EN);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
}

static void _starttransfer_up(struct print_info *print_info, int up_mem_index, unsigned int blk_count)
{
	void * reg_addr =  NULL;

	/* 将ph0起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH0);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)print_info->mem_info[up_mem_index].raw_addr));

	/* 将ph1起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH1);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)print_info->mem_info[up_mem_index].raw_addr + PH1_OFFSET));

	/* 将ph0和ph1地址空间大小写入fpga寄存器，单位1kb */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BLOCK_SIZE);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* 启动fpga写 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN); 
	alt_write_word(reg_addr,  _F2SM_FR_START_UP);	
	alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
}


static void _print_enable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINT_OPEN);
	alt_write_word(reg_addr, 0x1);
}

static void _print_disable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, PRINT_OPEN);
	alt_write_word(reg_addr, 0x0);
}

static void _config_master(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_INDICATOR);
	alt_write_word(reg_addr, 0x1);	
}

static void _config_slave(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_INDICATOR);
	alt_write_word(reg_addr, 0x0);	
}

static void _sync_sim_enable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, MASTER_SYNC_SIM_EN);
	alt_write_word(reg_addr, 0x1);
}


static void _test_data_enable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_TEST_DATA_EN);
	alt_write_word(reg_addr, 0x1);
}

static void _loop_test_enable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_LOOP_TEST_EN);
	alt_write_word(reg_addr, 0x1);
}

static void _base_image_enable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_BASE_IMAGE_EN);
	alt_write_word(reg_addr, 0x1);
}


static void _base_image_disable(struct print_info *print_info)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_BASE_IMAGE_EN);
	alt_write_word(reg_addr, 0x0);
}


static void _config_raster_sim_params(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_RASTER_SEL);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_RASTER_SIM_PERIOD);
	alt_write_word(reg_addr, regs[1]);	

}

static void _config_pd_sim_params(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_PD_SEL);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_PD_REVERSE);
	alt_write_word(reg_addr, regs[1]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_PD_SIM_NUMBER);
	alt_write_word(reg_addr, regs[2]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_PD_SIM_LENGTH);
	alt_write_word(reg_addr, regs[3]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_PD_SIM_INTERVAL);
	alt_write_word(reg_addr, regs[4]);
}


static void _config_divisor_params(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_DPI_D_FACTOR);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_DPI_M_FACTOR);
	alt_write_word(reg_addr, regs[1]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_MASTER_DPI_T_FACTOR);
	alt_write_word(reg_addr, regs[2]);	
}

static void _config_job_params(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_WORK_MODE);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PRINT_DIRECTION);
	alt_write_word(reg_addr, regs[1]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_INK_DROPLET_MODE);
	alt_write_word(reg_addr, regs[2]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_JOB_FIRE_NUM);
	alt_write_word(reg_addr, regs[3]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_DDR_BUFF_DEEP);
	alt_write_word(reg_addr, regs[4]);
}

static void _config_print_offset(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PH_OFFSET);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_0_31);
	alt_write_word(reg_addr, regs[1]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_32_63);
	alt_write_word(reg_addr, regs[2]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_64_95);
	alt_write_word(reg_addr, regs[3]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_96_127);
	alt_write_word(reg_addr, regs[4]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_128_159);
	alt_write_word(reg_addr, regs[5]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_160_191);
	alt_write_word(reg_addr, regs[6]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_192_223);
	alt_write_word(reg_addr, regs[7]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_CH_OFFSET_224_255);
	alt_write_word(reg_addr, regs[8]);
}

static void _config_fire_delay(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_DPI_DELAY_NUM_PH0);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_SUB_DELAY_NUM_PH0);
	alt_write_word(reg_addr, regs[1]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_DPI_DELAY_NUM_PH1);
	alt_write_word(reg_addr, regs[2]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_SUB_DELAY_NUM_PH1);
	alt_write_word(reg_addr, regs[3]);
}


static void _config_nozzle_switch(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_SW_INIT_EN);
	alt_write_word(reg_addr, regs[0]);
	
	usleep(10000);
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_SW_RW_CPU);
	alt_write_word(reg_addr, regs[1]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_SW_ADDR_CPU);
	alt_write_word(reg_addr, regs[2]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_SW_WDATA_CPU);
	alt_write_word(reg_addr, regs[3]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, I_SW_RDATA0_CPU);
	alt_write_word(reg_addr, regs[4]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, I_SW_RDATA1_CPU);
	alt_write_word(reg_addr, regs[5]);

}

static void _config_search_label_params(struct print_info *print_info, unsigned long *regs)
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PD_CHK_THRESHOLD);
	alt_write_word(reg_addr, regs[0]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PD_SERACH_MODE);
	alt_write_word(reg_addr, regs[1]);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PD_SEARCH_INTERVAL);
	alt_write_word(reg_addr, regs[2]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PD_SEARCH_WINDOW);
	alt_write_word(reg_addr, regs[3]);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(print_info->virtual_base, O_PD_VIRTUAL_EN);
	alt_write_word(reg_addr, regs[4]);
}


void init_print_info(struct print_info *print_info)
{	
	int i;

	print_info->up_fd = -1;
	print_info->down_fd = -1;
	print_info->mem_fd = -1;
	print_info->virtual_base = NULL;
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
	print_info->config_raster_sim_params = _config_raster_sim_params;
	print_info->config_pd_sim_params = _config_pd_sim_params;
	print_info->config_divisor_params = _config_divisor_params;
	print_info->config_job_params = _config_job_params;
	print_info->config_print_offset = _config_print_offset;
	print_info->config_fire_delay = _config_fire_delay;
	print_info->config_nozzle_switch = _config_nozzle_switch;
	print_info->config_search_label_params = _config_search_label_params;
}

