
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "F2smIntf.h"
#include "alt_f2sm_regs.h"


#define FPGAR_DEV "/dev/fpgar_ram"

CF2smIntf::CF2smIntf():
m_fd(-1),
virtual_base(NULL)
{
    //memset(mem_info, 0, sizeof(f2sm_mem_info_t) * F2SM_MEM_NUMS);
}

CF2smIntf::~CF2smIntf()
{

}

void CF2smIntf::Init()
{
    int fd = open(FPGAR_DEV, /*O_RDONLY*/O_RDWR);

    if( fd < 0) 
    {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }

    m_fd = fd;


	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
	{
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(m_fd);
		exit(-1);
    }

    mem_info[0].raw_addr = (void *)MEM_0_PHY_ADDR;
    mem_info[0].mem_size = MEM_0_SIZE;

    mem_info[1].raw_addr = (void *)MEM_1_PHY_ADDR;
    mem_info[1].mem_size = MEM_1_SIZE;

    //-----GENERATE ADRESSES TO ACCESS FPGA MEMORY AND DMACS FROM PROCESSOR-----//
    virtual_base = mmap( NULL, MMAP_SPAN, ( PROT_READ | PROT_WRITE ),MAP_SHARED, fd, MMAP_BASE );
    if( virtual_base == MAP_FAILED ) 
    {
        close( fd );
        close(m_fd);
        exit(-1);
    }


    close(fd);
}

void CF2smIntf::testinit()
{
    void * reg_addr =  NULL;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 37);
    alt_write_word(reg_addr,  0x0);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 36);
    alt_write_word(reg_addr,  0x1);

    return;

}

void CF2smIntf::ResetCounter()
{
    void * reg_addr =  NULL;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 26);
    alt_write_word(reg_addr,  0x1);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 26);
    alt_write_word(reg_addr,  0x0);

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 26);
    alt_write_word(reg_addr,  0x2);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 26);
    alt_write_word(reg_addr,  0x0);
	
    return;

}

void CF2smIntf::Close()
{

    if(virtual_base)
    {
        // ------clean up our memory mapping and exit -------//
        if( munmap( virtual_base, MMAP_SPAN ) != 0 ) 
        {
            printf( "ERROR: munmap() failed...\n" );
            if(m_fd != -1)
            {
                close(m_fd);
            }
            return;
        }
        virtual_base = NULL;
    }

    if(m_fd != -1)
    {
        close(m_fd);
    }
    m_fd = -1;
}

// After data is filled in mem
void CF2smIntf::StartTransfer(int mem_index, unsigned int seed1, unsigned int seed2, int shot_count)   
{
    void * reg_addr =  NULL;

	/* 将ph1起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 2);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index].raw_addr));

	/* 将ph2起始地址写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 4);
    alt_write_word(reg_addr, (unsigned int)((char *)mem_info[mem_index].raw_addr + 0x00800000));

	/* 将ph1种子写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 3);
    alt_write_word(reg_addr, seed1);

	/* 将ph2种子写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 5);
    alt_write_word(reg_addr, seed2);

	/* 将ph1和ph2地址空间大小写入fpga寄存器 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 1);
    alt_write_word(reg_addr, shot_count & 0xFFFF);

	/* 启动fpga读 */
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 0);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 0);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
	
    return;
}

bool CF2smIntf::CheckResult(u_int32_t u32WantLen)
{
    u_int32_t ret;
    void * reg_addr = NULL;


	/* 检查ph1 fpga读取的数据是否有错 */
    ret = (u_int32_t)IORD_F2SM_FR_CHECK_REG_PH1(virtual_base);
    if(ret != _F2SM_FR_CHECK_VALUE_OK_PH1)
    {
        printf("ph1 CheckResult Value error (0x%08x)\n",ret);
        return false;
    }
	
	/* 检查ph2 fpga读取的数据是否有错 */
    ret = (u_int32_t)IORD_F2SM_FR_CHECK_REG_PH2(virtual_base);
    if(ret != _F2SM_FR_CHECK_VALUE_OK_PH2)
    {
        printf("ph2 CheckResult Value error (0x%08x)\n",ret);
        return false;
    }	

	/* 检查ph1 fpga读取或什么的长度是否一直 */
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

	/* 检查ph2 fpga读取或什么的长度是否一直 */
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
	 * 延时10ms再去读38,39,40
	 */
	usleep(10000);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 38);
	ret = alt_read_word(reg_addr);
    if(ret != u32WantLen)
    {
        printf("ph1 CheckResult 38 error (0x%08x)\n",ret);
        return false;
    }	

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 39);
	ret = alt_read_word(reg_addr);
    if(ret != u32WantLen)
    {
        printf("ph2 CheckResult 39 error (0x%08x)\n",ret);
        return false;
    }

	reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, 40);
	ret = alt_read_word(reg_addr);
    if(ret != u32WantLen)
    {
        printf("ph CheckResult 40 error (0x%08x)\n",ret);
        return false;
    }
    return true;
}
