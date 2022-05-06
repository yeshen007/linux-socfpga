
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "F2smIntf.h"

#include "F2smIntf.h"
#include "alt_f2sm_regs.h"
#include "FpgaRegs.h"

#define FDDR_DEV "/dev/fddr_ram"

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
    int fd = open(FDDR_DEV, /*O_RDONLY*/O_RDWR);

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

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRINT_OPEN);
    alt_write_word(reg_addr,  0x0);

    return;

}

void CF2smIntf::ResetErrCounter()
{
    void * reg_addr =  NULL;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, READ_CHECK_ERR_CLEAN);
    alt_write_word(reg_addr,  _F2SM_FDDR_ERESET_UP);
    alt_write_word(reg_addr,  _F2SM_FDDR_ERESET_DOWN);
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

void CF2smIntf::StartWrite(int seed)
{
    void * reg_addr =  NULL;

    ResetErrCounter();

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, PRBS_SEED);
    alt_write_word(reg_addr,  (unsigned int)seed);

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, WRITE_BEGIN);
    alt_write_word(reg_addr,  _F2SM_FDDRW_START_UP);
    //alt_write_word(reg_addr,  _F2SM_FDDRW_START_DOWN);

    return;
}

void CF2smIntf::StartRead()
{
    void * reg_addr =  NULL;
    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, READ_BEGIN);
    alt_write_word(reg_addr,  _F2SM_FDDRR_START_UP);
    //alt_write_word(reg_addr,  _F2SM_FDDRR_START_DOWN);

    return;
}

bool CF2smIntf::CheckResult()  // Read result
{
    unsigned int result = 0;
    unsigned int errind = 0;

    void * reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, READ_CHECK_ERR_INDEX);
    result = alt_read_word(reg_addr);
    errind = result & _F2SM_FDDR_ERR_IND_MASK;

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, READ_CHECK_ERR_CNT);
    result = alt_read_word(reg_addr);

    if(errind == _F2SM_FDDR_ERR_IND)   // Error
    {
        printf("CheckResult has error, total: %d \n", result);
        return false;
    }

    return true;
}

bool CF2smIntf::IsReady()
{
    unsigned int result = 0;
    void * reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, I_PLL_PHCLK_LOCKED_FPGA_CLK_100_LOCK);
    result = alt_read_word(reg_addr);
    result &= _F2SM_FDDR_SYSCLOCK_RDY_MASK;
    if(result != _F2SM_FDDR_SYSCLOCK_RDY)
    {
        printf("Fail！ F2SM_FDDR_SYSCLOCK_RDY_REG not OK, v: %x \n", result);
        return false;
    }

    reg_addr = __IO_CALC_ADDRESS_NATIVE(virtual_base, LOCAL_INIT_DONE_LOCAL_CAL_SUCCESS_LOCAL_CAL_FAIL);
    result = alt_read_word(reg_addr);
    result &= _F2SM_FDDR_DDR_RDY_MASK;
    if(result != _F2SM_FDDR_DDR_RDY)
    {
        printf("Fail！ F2SM_FDDR_DDR_RDY_REG not OK, v: %x \n", result);
        return false;
    }

    return true;
}
