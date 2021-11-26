
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <asm/io.h>		    // For ioremap and ioread32 and iowrite32

#include "alt_f2sm_regs.h"
#include "f2sm_reg_test.h"

#define F2SM_REG_TEST_TIMES 10

//static int reg_nums = 20;
//static int reg_nos[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,253,254,255};  // 0# WO 253# RO
//static int reg_mask_bits[] = {0,1,16,16,32,16,32,32,32,1,1,32,32,32,32,32,32,0,32,32};

static int reg_nums = 10;
static int reg_nos[] = {1,2,3,4,5,6,7,8,510,511};  
static int reg_mask_bits[] = {16,32,32,32,32,1,16,32,32,0};

static void *f2sm_mmap_base = NULL;

static unsigned int f2sm_get_mask(int nBits)
{
    unsigned int ret = 0x0;
    switch( nBits)
    {
        case 32:
            ret = 0xFFFFFFFF;
            break;
        case 16:
            ret = 0xFFFF;
            break;
        case 1:
            ret = 0x1;
            break;
        case 0:
            ret = 0x0;
            break;
        default:
            if(nBits < 32)
            {
                ret = 1<<nBits;
                ret -= 1;
            }
            else
            {
                ret = 0;
            }
            break; 
    }
    return ret;
}

static void test_reg(int indx)
{
    int reg_no = reg_nos[indx];
    unsigned int mask = f2sm_get_mask(reg_mask_bits[indx]);

    unsigned int input = (unsigned int)rand();  // 0~32767
    unsigned int check = input & mask;
    unsigned int result = 0;
    void * reg_addr = __IO_CALC_ADDRESS_NATIVE(f2sm_mmap_base,reg_no);

    alt_write_word(reg_addr, input);
    result = alt_read_word(reg_addr);

    if(reg_no == 511)
    {
        //check = 0x20211012;
        printf("VersionDate: Reg %d read 0x%08x ,try write 0x%08x\n", reg_no, result, input);
        return;
    }
    else if(result != check)
    {
        printf("Fail！ Reg %d write0x%08x, read 0x%08x \n", reg_no,input,result);
    }
    else 
    {
        printf("OK！ Reg %d write0x%08x, read 0x%08x \n", reg_no,input,result);
    }
}

void f2sm_reg_test()
{
   int fd;
   int i,idx;
   srand((unsigned)time(NULL));

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
	{
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
		exit(-1);
    }

    //-----GENERATE ADRESSES TO ACCESS FPGA MEMORY AND DMACS FROM PROCESSOR-----//
    f2sm_mmap_base = mmap( NULL, MMAP_SPAN, ( PROT_READ | PROT_WRITE ),MAP_SHARED, fd, MMAP_BASE );
    if( f2sm_mmap_base == MAP_FAILED ) 
    {
        fprintf(stderr, "mmap -1 Error, at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close( fd );
        exit(-1);
    }

    close(fd);

    	test_reg(8);
	test_reg(9);
     //for(i=0; i< F2SM_REG_TEST_TIMES; i++)
     //{
         //for(idx = 0; idx< reg_nums; idx++)
         //{
             //test_reg(idx);
         //}
     //}

    if(f2sm_mmap_base)
    {
        // ------clean up our memory mapping and exit -------//
        if( munmap( f2sm_mmap_base, MMAP_SPAN ) != 0 ) 
        {
            printf( "ERROR: munmap() failed...\n" );
            return;
        }
        f2sm_mmap_base = NULL;
    }

    return;
}

int main(void)
{
    f2sm_reg_test();
    return 0;
}
