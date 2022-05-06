#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "YxmIntf.h"
#include "alt_f2sm_regs.h"
#include "FpgaRegs.h"


//misc device
#define UP_DEV	 "/dev/up_dev"
#define DOWN_DEV "/dev/down_dev"

#define MEM_PHY_UP 0x30000000
#define MEM_PHY_UP_SIZE 0x02000000
#define MEM_PHY_DOWN 0x32000000
#define MEM_PHY_DOWN_SIZE 0x02000000
#define PH1_OFFSET 0x01000000

#define UP_MEM_INDEX 	0
#define DOWN_MEM_INDEX 	1

#define DEFAULT_VAL 0


CYxmIntf::CYxmIntf():
up_fd(-1),
down_fd(-1),
virtual_base(NULL)
{
    
}

CYxmIntf::~CYxmIntf()
{

}


void CYxmIntf::Init()
{
    int fd;
	
	fd = open(UP_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }
    up_fd = fd;

    fd = open(DOWN_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
		close(up_fd);
        exit(-1);
    }
    down_fd = fd;


	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(down_fd);
		close(up_fd);
		exit(-1);
    }

    mem_info[UP_MEM_INDEX].raw_addr = (void *)MEM_PHY_UP;
    mem_info[UP_MEM_INDEX].mem_size = MEM_PHY_UP_SIZE;

    mem_info[DOWN_MEM_INDEX].raw_addr = (void *)MEM_PHY_DOWN;
    mem_info[DOWN_MEM_INDEX].mem_size = MEM_PHY_DOWN_SIZE;

    //-----GENERATE ADRESSES TO ACCESS FPGA MEMORY AND DMACS FROM PROCESSOR-----//
    virtual_base = mmap(NULL, MMAP_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, MMAP_BASE);
    if (virtual_base == MAP_FAILED) {
        close(fd);
        close(down_fd);
		close(up_fd);
        exit(-1);
    }

    close(fd);
}


void CYxmIntf::Close()
{
    if (virtual_base) {
        // ------clean up our memory mapping and exit -------//
        if (munmap( virtual_base, MMAP_SPAN ) != 0) {
            printf( "ERROR: munmap() failed...\n" );
            if (down_fd != -1)
                close(down_fd);
            down_fd = -1;			
            if (up_fd != -1)
                close(up_fd);
            up_fd = -1;
            return;
        }
        virtual_base = NULL;
    }
	if (down_fd != -1)
		close(down_fd);
	down_fd = -1;			
	if (up_fd != -1)
		close(up_fd);
	up_fd = -1;
}

/* 上行 */
void CYxmIntf::StartTransfer_up(int up_mem_index, unsigned int blk_count)   
{
    void * reg_addr = NULL;

	/* 将ph0起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH0);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[up_mem_index].raw_addr));

	/* 将ph1起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BASE_PH1);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[up_mem_index].raw_addr + PH1_OFFSET));

	/* 将ph0和ph1地址空间大小写入fpga寄存器，单位1kb */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_BLOCK_SIZE);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* 启动fpga写 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_WRITE_CONTROL_WRITE_EN);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}

/* 下行 */
void CYxmIntf::StartTransfer_down(int down_mem_index, unsigned int blk_count)   
{
    void * reg_addr =  NULL;

	/* 将ph0起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[down_mem_index].raw_addr));

	/* 将ph1起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[down_mem_index].raw_addr + PH1_OFFSET));


	/* 将ph0和ph1地址空间大小写入fpga寄存器，单位3kb */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* 启动fpga读 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_EN);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}



void CYxmIntf::ConfigMaster()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_INDICATOR);
	alt_write_word(reg_addr, 0x1);
}

void CYxmIntf::ConfigSlave()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_INDICATOR);
	alt_write_word(reg_addr, 0x0);
}

void CYxmIntf::PrintEn()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINT_OPEN);
	alt_write_word(reg_addr, 0x1);
}

void CYxmIntf::PrintDi()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINT_OPEN);
	alt_write_word(reg_addr, 0x0);
}

void CYxmIntf::SyncSimEn()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, MASTER_SYNC_SIM_EN);
	alt_write_word(reg_addr, 0x1);
}


void CYxmIntf::TestDataEn()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_TEST_DATA_EN);
	alt_write_word(reg_addr, 0x1);
}

void CYxmIntf::LoopTestEn()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_LOOP_TEST_EN);
	alt_write_word(reg_addr, 0x1);
}


void CYxmIntf::BaseImageEn()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_BASE_IMAGE_EN);
	alt_write_word(reg_addr, 0x1);
}

void CYxmIntf::BaseImageDi()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_BASE_IMAGE_EN);
	alt_write_word(reg_addr, 0x0);
}

void CYxmIntf::WaitBaseImageTranferComplete()
{
	void * reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, I_BASE_IMAGE_TRANFER_COMPLETE);
	while (alt_read_word(reg_addr) != 1)
		usleep(1000);
	printf("base image transfer complete\n");	
}


void CYxmIntf::ConfigRasterSim()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_RASTER_SEL);
	alt_write_word(reg_addr, 0x1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_RASTER_SIM_PERIOD);
	alt_write_word(reg_addr, 55);	
}

void CYxmIntf::ConfigPdSim()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_PD_SEL);
	alt_write_word(reg_addr, 0x1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_PD_REVERSE);
	alt_write_word(reg_addr, DEFAULT_VAL);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_PD_SIM_NUMBER);
	alt_write_word(reg_addr, 1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_PD_SIM_LENGTH);
	alt_write_word(reg_addr, 100);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_PD_SIM_INTERVAL);
	alt_write_word(reg_addr, 4096);
}


void CYxmIntf::ConfigDivosr()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_DPI_D_FACTOR);
	alt_write_word(reg_addr, 3);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_DPI_M_FACTOR);
	alt_write_word(reg_addr, 1);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_MASTER_DPI_T_FACTOR);
	alt_write_word(reg_addr, 8);
}

void CYxmIntf::ConfigJob()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_WORK_MODE);
	alt_write_word(reg_addr, 0x2);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PRINT_DIRECTION);
	alt_write_word(reg_addr, DEFAULT_VAL);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_INK_DROPLET_MODE);
	alt_write_word(reg_addr, DEFAULT_VAL);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_JOB_FIRE_NUM);
	alt_write_word(reg_addr, 4096);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_DDR_BUFF_DEEP);
	alt_write_word(reg_addr, 4096 * 8);	
}


void CYxmIntf::ConfigOffset()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PH_OFFSET);
	alt_write_word(reg_addr, 1200);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_0_31);
	alt_write_word(reg_addr, (20<<16 | 0));	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_32_63);
	alt_write_word(reg_addr, (84<<16 | 64));
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_64_95);
	alt_write_word(reg_addr, (114<<16 | 94));	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_96_127);
	alt_write_word(reg_addr, (178<<16 | 158));
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_128_159);
	alt_write_word(reg_addr, (206<<16 | 186));	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_160_191);
	alt_write_word(reg_addr, (270<<16 | 250));
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_192_223);
	alt_write_word(reg_addr, (300<<16 | 280));	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_CH_OFFSET_224_255);
	alt_write_word(reg_addr, (364<<16 | 344));
}

void CYxmIntf::ConfigFire()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_DPI_DELAY_NUM_PH0);
	alt_write_word(reg_addr, DEFAULT_VAL);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_SUB_DELAY_NUM_PH0);
	alt_write_word(reg_addr, DEFAULT_VAL);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_DPI_DELAY_NUM_PH1);
	alt_write_word(reg_addr, DEFAULT_VAL);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_SUB_DELAY_NUM_PH1);
	alt_write_word(reg_addr, DEFAULT_VAL);
}

void CYxmIntf::ConfigSwitch()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_SW_INIT_EN);
	alt_write_word(reg_addr, DEFAULT_VAL);
	
	usleep(10000);
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_SW_RW_CPU);
	alt_write_word(reg_addr, DEFAULT_VAL);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_SW_ADDR_CPU);
	alt_write_word(reg_addr, DEFAULT_VAL);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_SW_WDATA_CPU);
	alt_write_word(reg_addr, DEFAULT_VAL);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, I_SW_RDATA0_CPU);
	alt_write_word(reg_addr, DEFAULT_VAL);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, I_SW_RDATA1_CPU);
	alt_write_word(reg_addr, DEFAULT_VAL);

}

void CYxmIntf::ConfigLabel()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PD_CHK_THRESHOLD);
	alt_write_word(reg_addr, 100);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PD_SERACH_MODE);
	alt_write_word(reg_addr, 1);	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PD_SEARCH_INTERVAL);
	alt_write_word(reg_addr, 4096);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PD_SEARCH_WINDOW);
	alt_write_word(reg_addr, 1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, O_PD_VIRTUAL_EN);
	alt_write_word(reg_addr, 1);
}

void CYxmIntf::PrintRegs()
{
	void * reg_virtual_addr;
	void * reg_phy_addr;
	u_int32_t reg_val;
	u_int32_t reg_index;

	FILE *regs_value_file;
	regs_value_file = fopen("regs-print.txt", "w+");
	if (regs_value_file == NULL)
		fprintf(stderr, "open regs-print.txt error\n");
	

	/* 打印50-355号寄存器 */
	for (reg_index = 50; reg_index <= 355; reg_index++) {
		reg_phy_addr = __IO_CALC_ADDRESS_NATIVE(HPS_FPGA_BRIDGE_BASE, reg_index); 
		reg_virtual_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, reg_index);
		reg_val = alt_read_word(reg_virtual_addr);
		fprintf(regs_value_file, "reg %u -- addr %p -- decvalue %d -- hexvalue 0x%08x\n", 
				reg_index, reg_phy_addr, (int)reg_val, reg_val);
	}

	fclose(regs_value_file);
}




