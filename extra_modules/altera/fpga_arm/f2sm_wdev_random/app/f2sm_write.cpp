
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

#define MIN_START_VALUE 1
#define TEST_TIMES 30000
//#define TEST_TIMES 30
#define CLK_REP_TESTS 1000 //repetitions to get clock statistics

//#define alt_write_word(dest, src)      iowrite32((uint32_t) src, (void *) dest)
//#define alt_read_word(src)        ioread32(src)
//#define     alt_setbits_word(dest, bits)        (alt_write_word(dest, alt_read_word(dest) | (bits)))
//#define     alt_clrbits_word(dest, bits)        (alt_write_word(dest, alt_read_word(dest) & ~(bits)))

// 64bit-8Byte, continuous number, len is 8-Ns
void PrepareData(int *pBuf, int nIntLen,int randv)
{
    int count = MIN_START_VALUE;    // Reason - Register begin with N-1, to be simple here start with 1
    //int *pPos = pBuf;
    for(int i = 0; i < nIntLen; )
    {
        pBuf[i] = count++;
        pBuf[i+1] = randv;
        i+=2;
    }
}

#define READ_PRE_1226 1

void do_test(CF2smIntf &f2sm, char *pDataBuf)
{
    int frb_size = 12*1024;
    int frb_count = 85;
    int blk_size = frb_size * frb_count;       // 85 * 12K
    int the_fd = f2sm.m_fd;

    //Start
    int rand_l;
    int rand_h;
    rand_l = (unsigned int)rand();  // 0~32767
    rand_h = rand_l + 1;  // PRBS Multi-channel
    printf("Rand v: 0x%08x-%08x \n",rand_h,rand_l);

    int nByte = 8*1024*1024;  // max 
    unsigned int *pPRBSBuf = (unsigned int *)aligned_alloc(4096,nByte); // 30MB    
    if(pPRBSBuf == NULL)
    {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }

    int nInt =  blk_size/4;
    *pPRBSBuf = rand_l;
    *(pPRBSBuf+1) = rand_h;
    GenPrbsArray(rand_l, 2, pPRBSBuf+2, nInt-2);


    // interrupt process
    int times = 0;
    //int counter = 0;
    int ret;
    int rand_cnt_m;
    int rand_cnt_a;
    int rand_cnt_b;
    fd_set rd_fds;
    f2sm.StartTransfer(rand_l,rand_h,frb_count);

	while (1) {
		// 清空文件描述符集合
		FD_ZERO(&rd_fds);
		FD_SET(f2sm.m_fd, &rd_fds);

        memset(pDataBuf,0,blk_size);
		// ret 待处理事件总数
		ret = select(the_fd+1, &rd_fds, NULL, NULL, NULL);
		if (ret > 0) {
			// 检测读管道文件描述符是否存储在tmp_fds中
			// 检测此文件描述符中是否有事件发生
			if (FD_ISSET(the_fd, &rd_fds)) 
            {
                //counter = 0;
                //memset(&read_info,0,sizeof(read_info));
				int nRead = read(the_fd, pDataBuf, blk_size);

                if(nRead == blk_size)
                {
                    if(!f2sm.CheckResult(rand_l, pDataBuf,(char *)pPRBSBuf,blk_size))   //check error
                    {
                        break;
                    }
                    times++;
                    //if(times == TEST_TIMES)
                    //    break;
                    usleep(30*1000);

                    rand_h = rand();  // 0~32767
                    rand_l = rand()%0xffff;  // 0~32767

                    rand_cnt_m = rand()%5 + 1;
                    rand_cnt_a = rand_cnt_m * 10;
                    rand_cnt_b = rand()%9;
                    frb_count = 85* rand_cnt_m + rand_cnt_a + rand_cnt_b;
                    //frb_count = 85;
                    blk_size = frb_count * 12 * 1024; 

                    rand_l = (unsigned int)rand();  // 0~32767
                    rand_h = rand_l + 1;  // PRBS Multi-channel
                    //printf("Rand: 0x%08x-%08x \n",rand_h,rand_l);
                    printf("frb_cnt: %d \n",frb_count);
                    fflush(stdout);
                    nInt =  blk_size/4;
                    *pPRBSBuf = rand_l;
                    *(pPRBSBuf+1) = rand_h;
                    GenPrbsArray(rand_l, 2, pPRBSBuf+2, nInt-2);

                    f2sm.StartTransfer(rand_l,rand_h,frb_count);
                }
                else
                {
                    printf("Read error!\n");
                    break;
                }
			}
		}
	}

    printf("%d * %d B\n",times, blk_size);
    fflush(stdout); 

    free(pPRBSBuf);
}

int f2sm_write_test(void)
{
    //f2sm_reg_test();
    //return 0;
    srand((unsigned)time(NULL));

    //printf("Start!\n");
    int nByte = 30*1024*1024;
    char *pDataBuf = (char *)aligned_alloc(4096,nByte); // 30MB    
    if(pDataBuf == NULL)
    {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }


    CF2smIntf f2sm;
    f2sm.Init();

    do_test(f2sm,pDataBuf);

    f2sm.Close();

    free(pDataBuf);

    return 0;
}

int main(void)
{
    f2sm_write_test();

    return 0;
}