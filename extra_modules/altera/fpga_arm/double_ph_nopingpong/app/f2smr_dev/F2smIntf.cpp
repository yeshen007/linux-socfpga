
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "F2smIntf.h"
#include "alt_f2sm_regs.h"
#include "FpgaRegs.h"


#define UP_DEV	 "/dev/up_dev"
#define DOWN_DEV "/dev/down_dev"


#define MEM_PHY_UP 0x30000000
#define MEM_PHY_UP_SIZE 0x02000000
#define MEM_PHY_DOWN 0x32000000
#define MEM_PHY_DOWN_SIZE 0x02000000
#define PH1_OFFSET 0x01000000


#define UP_MEM_INDEX 	0
#define DOWN_MEM_INDEX 	1


CF2smIntf::CF2smIntf():
up_fd(-1),
down_fd(-1),
virtual_base(NULL)
{
    
}

CF2smIntf::~CF2smIntf()
{

}


void CF2smIntf::Init()
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


void CF2smIntf::Close()
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



void CF2smIntf::testinit()
{
    void * reg_addr =  NULL;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINT_OPEN);
    alt_write_word(reg_addr,  0x0);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRBS_EN);
    alt_write_word(reg_addr,  0x1);

    return;

}

void CF2smIntf::ResetCounter()
{
    void * reg_addr =  NULL;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CLEAN_PH0_PH1);
    alt_write_word(reg_addr,  0x1);
    alt_write_word(reg_addr,  0x0);

    alt_write_word(reg_addr,  0x2);
    alt_write_word(reg_addr,  0x0);
	
    return;
}



// After data is filled in mem
void CF2smIntf::StartTransfer_down_seed(unsigned int seed1, unsigned int seed2, int blk_count)   
{
    void * reg_addr =  NULL;

	/* ???ph1??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH0);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)mem_info[DOWN_MEM_INDEX].raw_addr));

	/* ???ph2??????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BASE_PH1);
    alt_write_word(reg_addr, (unsigned int)((unsigned char *)mem_info[DOWN_MEM_INDEX].raw_addr + PH1_OFFSET));

	/* ???ph1????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH0);
    alt_write_word(reg_addr, seed1);

	/* ???ph2????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_SEED_PH1);
    alt_write_word(reg_addr, seed2);

	/* ???ph1???ph2????????????????????????fpga????????? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_BLOCK_SIZE);
    alt_write_word(reg_addr, blk_count & 0xFFFF);

	/* ??????fpga??? */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINTER_DATA_MASTER_READ_CONTROL_READ_EN);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}




bool CF2smIntf::CheckResult(u_int32_t u32WantLen)
{
    u_int32_t ret;
    void * reg_addr = NULL;


	/* ??????ph1 fpga??????????????????????????? */
    ret = (u_int32_t)IORD_F2SM_FR_CHECK_REG_PH1(virtual_base);
    if(ret != _F2SM_FR_CHECK_VALUE_OK_PH1)
    {
        printf("ph1 CheckResult Value error (0x%08x)\n",ret);
        return false;
    }
	
	/* ??????ph2 fpga??????????????????????????? */
    ret = (u_int32_t)IORD_F2SM_FR_CHECK_REG_PH2(virtual_base);
    if(ret != _F2SM_FR_CHECK_VALUE_OK_PH2)
    {
        printf("ph2 CheckResult Value error (0x%08x)\n",ret);
        return false;
    }	

	/* ??????ph1 fpga???????????????????????????????????? */
    ret = (u_int32_t)IORD_F2SM_FR_RDLEN_REG_PH1(virtual_base);
    if(ret != u32WantLen)
    {
        printf("ph1 CheckResult RDLEN error (0x%08x)\n",ret);
        return false;
    }
    ret = (u_int32_t)IORD_F2SM_FR_DDLEN_REG_PH1(virtual_base);
    if(ret != u32WantLen)
    {
        printf("ph1 CheckResult DDLEN error (0x%08x)\n",ret);
        return false;
    }

	/* ??????ph2 fpga???????????????????????????????????? */
    ret = (u_int32_t)IORD_F2SM_FR_RDLEN_REG_PH2(virtual_base);
    if(ret != u32WantLen)
    {
        printf("ph2 CheckResult RDLEN error (0x%08x)\n",ret);
        return false;
    }
    ret = (u_int32_t)IORD_F2SM_FR_DDLEN_REG_PH2(virtual_base);
    if(ret != u32WantLen)
    {
        printf("ph2 CheckResult DDLEN error (0x%08x)\n",ret);
        return false;
    }	

    	/*
	 * ??????10ms?????????38,39,40
	 */
	usleep(10000);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, DMA_RAM_WRITE_DONE_BLOCK_CNT_PH0);
	ret = alt_read_word(reg_addr);
    if(ret != u32WantLen)
    {
        printf("ph1 CheckResult 38 error (0x%08x)\n",ret);
        return false;
    }	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, DMA_RAM_WRITE_DONE_BLOCK_CNT_PH1);
	ret = alt_read_word(reg_addr);
    if(ret != u32WantLen)
    {
        printf("ph2 CheckResult 39 error (0x%08x)\n",ret);
        return false;
    }

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, DMA_RAM_READ_DONE_BLOCK_CNT);
	ret = alt_read_word(reg_addr);
    if(ret != u32WantLen)
    {
        printf("ph CheckResult 40 error (0x%08x)\n",ret);
        return false;
    }
    return true;
}
