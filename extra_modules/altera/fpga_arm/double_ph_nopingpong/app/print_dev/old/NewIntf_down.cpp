#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "YxmIntf.h"
#include "alt_f2sm_regs.h"

//misc device
#define UP_DEV "/dev/up_dev"
#define DOWN_DEV "/dev/down_dev"


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

    mem_info[0].raw_addr = (void *)MEM_PHY_UP;
    mem_info[0].mem_size = MEM_PHY_UP_SIZE;

    mem_info[1].raw_addr = (void *)MEM_PHY_DOWN;
    mem_info[1].mem_size = MEM_PHY_DOWN_SIZE;

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


// After data is filled in mem
void CYxmIntf::StartTransfer(int mem_index, unsigned int seed1, unsigned int seed2, unsigned int blk_count)   
{
    void * reg_addr =  NULL;

	/* ???ph1??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 2);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index].raw_addr));

	/* ???ph2??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 4);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index].raw_addr + 0x00800000));

	/* ???ph1????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 3);
    alt_write_word(reg_addr, seed1);

	/* ???ph2????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 5);
    alt_write_word(reg_addr, seed2);

	/* ???ph1???ph2????????????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 1);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* ??????fpga??? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 6);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 6);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}

/* ?????? */
void CYxmIntf::StartTransfer_up(int mem_index_up, unsigned int blk_count)   
{
    void * reg_addr = NULL;

	/* ???ph1??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 8);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index_up].raw_addr));

	/* ???ph2??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 10);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index_up].raw_addr + PH1_OFFSET));

	/* ???ph1???ph2????????????????????????fpga??????????????????1kb */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 7);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* ??????fpga??? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 6);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 6);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}

/* ?????? */
void CYxmIntf::StartTransfer_down(int mem_index_down, unsigned int blk_count)   
{
    void * reg_addr =  NULL;

	/* ???ph1??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 2);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index_down].raw_addr));

	/* ???ph2??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 4);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index_down].raw_addr + PH1_OFFSET));


	/* ???ph1???ph2????????????????????????fpga??????????????????3kb */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 1);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* ??????fpga??? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 0);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 0);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}



void CYxmIntf::StartPrint()
{
	void * reg_addr =  NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 37);
	alt_write_word(reg_addr, 0x1);

}

void CYxmIntf::FinishPrint()
{
	void * reg_addr =  NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 37);
	alt_write_word(reg_addr, 0x0);

}

void CYxmIntf::Step1()
{
	void * reg_addr =  NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 104);
	alt_write_word(reg_addr, 0x1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 104);
	alt_write_word(reg_addr, 0x0);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 105);
	alt_write_word(reg_addr, 0x1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 105);
	alt_write_word(reg_addr, 0x0);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 37);
	alt_write_word(reg_addr, 0x0);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 53);
	alt_write_word(reg_addr, 3);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 54);
	alt_write_word(reg_addr, 1);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 55);
	alt_write_word(reg_addr, 8);
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 60);
	alt_write_word(reg_addr, 0x1);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 61);
	alt_write_word(reg_addr, 0x1);	
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 62);
	alt_write_word(reg_addr, 55);


}

void CYxmIntf::Step2()
{
	void * reg_addr =  NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 110);
	alt_write_word(reg_addr, 0x1);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 113);
	alt_write_word(reg_addr, 40000);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 70);
	alt_write_word(reg_addr, 0x1);		

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 71);
	alt_write_word(reg_addr, 0x0);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 72);
	alt_write_word(reg_addr, 0x1);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 73);
	alt_write_word(reg_addr, 160000);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 74);
	alt_write_word(reg_addr, 100);		

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 75);
	alt_write_word(reg_addr, 40000);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 80);
	alt_write_word(reg_addr, 100);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 90);
	alt_write_word(reg_addr, 1);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 91);
	alt_write_word(reg_addr, 40000);		

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 92);
	alt_write_word(reg_addr, 1);	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 93);
	alt_write_word(reg_addr, 0x1);	

}

void CYxmIntf::Step3()
{
	void * reg_addr =  NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 140);
	alt_write_word(reg_addr, 0x1);	

}

void CYxmIntf::Step3_1()
{
	void * reg_addr =  NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 102);
	alt_write_word(reg_addr, 40000);	
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 103);
	alt_write_word(reg_addr, 8);	
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 101);
	alt_write_word(reg_addr, 0x0);	
	alt_write_word(reg_addr, 0x1);	

}



void CYxmIntf::PrintRegs()
{
	void * reg_virtual_addr;
	void * reg_phy_addr;
	u_int32_t reg_val;
	u_int32_t reg_index;

	FILE *regs_value_file;
	regs_value_file = fopen("/root/regs-master.txt", "w+");
	if (regs_value_file == NULL)
		fprintf(stderr, "open regs-xxx.txt error\n");	

	/* ??????0-511???????????? */
	for (reg_index = 0; reg_index <= 511; reg_index++) {
		reg_phy_addr = __IO_CALC_ADDRESS_NATIVE(HPS_FPGA_BRIDGE_BASE, reg_index); 
		reg_virtual_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, reg_index);
		reg_val = alt_read_word(reg_virtual_addr);
		fprintf(regs_value_file, "reg %u -- addr %p -- decvalue %d -- hexvalue 0x%08x\n", 
				reg_index, reg_phy_addr, reg_val, reg_val);
	}

	fclose(regs_value_file);
}



void CYxmIntf::PullDown_52()
{
	void * reg_addr =  NULL;
	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 52);
	alt_write_word(reg_addr, 0);
}






