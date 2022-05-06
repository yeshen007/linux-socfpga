
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
//#include <linux/types.h>
//#include <asm/string.h>
#include <assert.h>
#include <string.h>
#include <asm/io.h>		    // For ioremap and ioread32 and iowrite32

#include "alt_f2sm_regs.h"

//#include "arm_neon.h"
#include "F2smIntf.h"
#include "utils.h"

//#include "pmu.h" //to measure time with PMU
//#include "statistics.h" //do some statistics (mean, min, max, variance)

#define MIN_START_VALUE 1
#define TEST_TIMES 300
//#define TEST_TIMES 30
#define CLK_REP_TESTS 1000 //repetitions to get clock statistics
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

//#define     alt_setbits_word(dest, bits)        (alt_write_word(dest, alt_read_word(dest) | (bits)))
//#define     alt_clrbits_word(dest, bits)        (alt_write_word(dest, alt_read_word(dest) & ~(bits)))

struct f2sm_read_info_t
{
	u_int32_t irq_count;
	int mem_index;
};

void fix_do_test(CF2smIntf &f2sm, int ost_frb_count, int tnd_frb_count)
{
   
	int frb_size = 3*1024;
	int one_ph_buf_size = 12 * 1024 * 1024;
	int buf_size = 2 * one_ph_buf_size;

    char *ost_pDataBuf = (char *)aligned_alloc(4096, buf_size); 
    if(ost_pDataBuf == NULL) {
        fprintf(stderr, "alloc 1st buf for data failed!\n");
        exit(-1);
    }
    char *tnd_pDataBuf = (char *)aligned_alloc(4096, buf_size); 
    if(tnd_pDataBuf == NULL) {
        fprintf(stderr, "alloc 2nd buf for data failed!\n");
        exit(-1);
    }

	unsigned int *ost_pPrbsBuff = (unsigned int *)ost_pDataBuf;
	unsigned int *tnd_pPrbsBuff = (unsigned int *)tnd_pDataBuf;

	unsigned int ost_randv = 0;
	unsigned int tnd_randv = 0;

	int ost_size = ost_frb_count * frb_size;	//must < one_ph_buf_size
	int tnd_size = tnd_frb_count * frb_size;	//must < one_ph_buf_size
	int ost_nInt = ost_size/4;
	int tnd_nInt = tnd_size/4;

	int ost_nSendByte = 0;
	int tnd_nSendByte = 0;


    int times = 0;
    int first = true;
    struct timeval tv_s;
    struct timeval tv_e;
    long duration = 0;
	int the_fd = f2sm.m_fd;
	struct f2sm_read_info_t read_info;
	u_int32_t u32SendCount = 0;

	/* gen seed and rand data */

	unsigned int ost_ph1_randv = (unsigned int)rand();
	unsigned int ost_ph2_randv = (unsigned int)rand();
	unsigned int tnd_ph1_randv = (unsigned int)rand();
	unsigned int tnd_ph2_randv = (unsigned int)rand();
	

	GenPrbsArray(ost_ph1_randv, 4, ost_pPrbsBuff, ost_nInt);
	GenPrbsArray(ost_ph2_randv, 4, (unsigned int *)((char *)ost_pPrbsBuff + one_ph_buf_size), ost_nInt);
	GenPrbsArray(tnd_ph1_randv, 4, tnd_pPrbsBuff, tnd_nInt);
	GenPrbsArray(tnd_ph2_randv, 4, (unsigned int *)((char *)tnd_pPrbsBuff + one_ph_buf_size), tnd_nInt);	

	//read -> write -> send -> check
	while (1) {
		/* read */
		memset(&read_info,0,sizeof(read_info));
		int nRead = read(the_fd, &read_info, sizeof(read_info));
		if (nRead == sizeof(read_info)) {		//read ok
			if (first) {
				printf("first read info %d %d,must won't wait\n",read_info.irq_count, read_info.mem_index);
				first = false;
				times++;
			} else {
				printf("normal read info %d %d\n",read_info.irq_count, read_info.mem_index);
				gettimeofday(&tv_e,NULL);
				duration = (tv_e.tv_sec - tv_s.tv_sec)*1000 + (tv_e.tv_usec - tv_s.tv_usec)/1000;
				printf("duration in send and reav used %lu ms\n", duration); 
				/* check */
				if(!f2sm.CheckResult(u32SendCount))  
					return;
				else
					printf("CheckResult ok\n");
				times++;
				if(times == TEST_TIMES) {
					printf("test done\n");
					break;	
				}

				/* gen seed and rand data */

				ost_ph1_randv = (unsigned int)rand();
				ost_ph2_randv = (unsigned int)rand();
				tnd_ph1_randv = (unsigned int)rand();
				tnd_ph2_randv = (unsigned int)rand();				
				GenPrbsArray(ost_ph1_randv, 4, ost_pPrbsBuff, ost_nInt);
				GenPrbsArray(ost_ph2_randv, 4, (unsigned int *)((char *)ost_pPrbsBuff + one_ph_buf_size), ost_nInt);
				GenPrbsArray(tnd_ph1_randv, 4, tnd_pPrbsBuff, tnd_nInt);
				GenPrbsArray(tnd_ph2_randv, 4, (unsigned int *)((char *)tnd_pPrbsBuff + one_ph_buf_size), tnd_nInt);				
			}

			
			if (times & 0x1) {		//1st
				/* 1st write */
				int ost_nSendByte = write(the_fd, ost_pDataBuf, ost_size);
				if (ost_nSendByte != ost_size) {
					printf("1st write error! %d : %d \n", ost_size, ost_nSendByte);
					return;
				}

				/* 1st send */
//				printf("press to 1st send:\n");
//				getchar();
				gettimeofday(&tv_s,NULL);
//				f2sm.StartTransfer(read_info.mem_index, ost_ph1_randv, ost_frb_count);
				f2sm.StartTransfer(read_info.mem_index, ost_ph1_randv, ost_ph2_randv, ost_frb_count);
				u32SendCount += ost_frb_count;
				if (u32SendCount > 0x0FFFFF00) {
					printf("test done\n");
					break;
				}
	
			} else {	//2nd
				/* 2nd write */
				int tnd_nSendByte = write(the_fd, tnd_pDataBuf, tnd_size);
				if(tnd_nSendByte != tnd_size) {
					printf("2nd write error! %d : %d \n", tnd_size, tnd_nSendByte);
					return;
				}

				/* 2nd send */
//				printf("press to 2nd send:\n");
//				getchar();
				gettimeofday(&tv_s,NULL);
//				f2sm.StartTransfer(read_info.mem_index, tnd_ph1_randv, tnd_frb_count);
				f2sm.StartTransfer(read_info.mem_index, tnd_ph1_randv, tnd_ph2_randv, tnd_frb_count);
				u32SendCount += tnd_frb_count;
				if (u32SendCount > 0x0FFFFF00) {
					printf("test done\n");
					break;
				}				
			}
	
		}else { //read error
			printf("Read error!\n");
			return;
		}
		/* read end */	
	}	
}

int fix_f2sm_read_test(void)
{
    CF2smIntf f2sm;


   	f2sm.Init();

	f2sm.testinit();

	fix_do_test(f2sm, 870, 136);


	f2sm.Close();


    return 0;
}

int main(void)
{
	fix_f2sm_read_test();

    return 0;
}
