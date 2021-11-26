
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

#if 0
    void *mem0 = mmap(0, mem_info[0].mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)mem_info[0].raw_addr);
    if(mem0 == (void *) -1)
	{
		fprintf(stderr, "mmap -0 Error, at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(m_fd);
		close(fd);
		exit(-1);
    }
    mem_info[0].mem_addr = mem0;

    void *mem1 = mmap(0, mem_info[1].mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)mem_info[1].raw_addr);
    if ( mem1 == (void*) -1) 
    {
		fprintf(stderr, "mmap -1 Error, at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(m_fd);
		close(fd);
        munmap(mem_info[0].mem_addr, mem_info[0].mem_size);
        mem_info[0].mem_addr = NULL;
        exit(-1);
    }
    mem_info[1].mem_addr = mem1;
#endif
    close(fd);
}

void CF2smIntf::ResetCounter()
{
    void * reg_addr =  NULL;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_RESETLEN_REG);
    alt_write_word(reg_addr,  _F2SM_FR_RESETLEN_UP);

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_RESETLEN_REG);
    alt_write_word(reg_addr,  _F2SM_FR_RESETLEN_DOWN);
    return;

}

void CF2smIntf::Close()
{
#if 0    
    munmap(mem_info[0].mem_addr, mem_info[0].mem_size);
    mem_info[0].mem_addr = NULL;
    munmap(mem_info[1].mem_addr, mem_info[1].mem_size);
    mem_info[1].mem_addr = NULL;
#endif
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
void CF2smIntf::StartTransfer(int mem_index, int min_l, int min_h,int shot_count)   // n * 12KB, 16bit valid
{
    void * reg_addr =  NULL;
    int offset = shot_count * 4 * 1024;
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_ADDR_REG);
    //printf("alt_write_word Reg %d Value： (0x%08x)\n",F2SM_FR_ADDR_REG,(unsigned int)mem_info[0].raw_addr);
    alt_write_word(reg_addr,  (unsigned int)((char *)mem_info[mem_index].raw_addr + offset));

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_SIZE_REG);
    //printf("alt_write_word Reg %d Value： (0x%08x)\n",F2SM_FR_SIZE_REG,shot_count);
    alt_write_word(reg_addr,  shot_count&0xFFFF);


    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_DATA_MINL_REG);
    alt_write_word(reg_addr, min_l);

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_DATA_MINH_REG);
    alt_write_word(reg_addr,  min_h);

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_START_REG);
    alt_write_word(reg_addr,  _F2SM_FR_START_UP);

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base,F2SM_FR_START_REG);
    alt_write_word(reg_addr,  _F2SM_FR_START_DOWN);
    return;
}

bool CF2smIntf::CheckResult(u_int32_t u32WantLen)
{
    u_int32_t ret;
    ret = (u_int32_t)IORD_F2SM_FR_CHECK_REG(virtual_base);
    if(ret != _F2SM_FR_CHECK_VALUE_OK)
    {
        printf("CheckResult Value error (0x%08x)\n",ret);
        return false;
    }

    ret = (u_int32_t)IORD_F2SM_FR_RDLEN_REG(virtual_base);
    if(ret != u32WantLen)
    {
        printf("CheckResult RDLEN error (0x%08x)\n",ret);
        return false;
    }

    ret = (u_int32_t)IORD_F2SM_FR_DDLEN_REG(virtual_base);
    if(ret != u32WantLen)
    {
        printf("CheckResult DDLEN error (0x%08x)\n",ret);
        return false;
    }

    return true;
}

