
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
# define likely(x)	__builtin_expect(!!(x), 1)
# define unlikely(x)	__builtin_expect(!!(x), 0)
//#define alt_write_word(dest, src)      iowrite32((uint32_t) src, (void *) dest)
//#define alt_read_word(src)        ioread32(src)
//#define     alt_setbits_word(dest, bits)        (alt_write_word(dest, alt_read_word(dest) | (bits)))
//#define     alt_clrbits_word(dest, bits)        (alt_write_word(dest, alt_read_word(dest) & ~(bits)))

struct f2sm_read_info_t
{
	u_int32_t irq_count;
	int mem_index;
};

unsigned long long int clk_read_average;

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

void do_test(CF2smIntf &f2sm, int frb_count, char *pDataBuf,int nLimit,int randv)
{
    int frb_size = 12*1024;
    int blk_size = frb_size * frb_count;       // 85 * 12K
    int offset = 0;

    struct timeval tv_s;
    struct timeval tv_e;
    long duration = 0;

    int the_fd = f2sm.m_fd;
    //int onv = 1;   // interrupt control
    //write(the_fd,&onv,sizeof(int));       // 开中断
    f2sm.ResetCounter();

    //Start
    unsigned int min_l;
    unsigned int min_h;
    u_int32_t u32SendCount = 0;
    char *pSrcPos = NULL;
    int nSendByte = 0;

    gettimeofday(&tv_s,NULL);


#if READ_PRE_1226
#else
    assert(nLimit >= blk_size);
    pSrcPos = (char *)pDataBuf+offset;

    //memcpy(pDestPos,pSrcPos,blk_size);
    nSendByte = write(the_fd,pSrcPos,blk_size);
    //assert(nSendByte == blk_size);
    if(nSendByte != blk_size)
    {
		printf("write error! %d : %d \n", blk_size, nSendByte);
        return;
    }

    //if(offset == 0)
    {
        min_l = MIN_START_VALUE-1;
        min_h = randv;
    }

    f2sm.StartTransfer(0,min_l,min_h,frb_count);
    offset += blk_size; //already
    if(offset + blk_size > nLimit)     //will
    {
        offset = 0;
    }

    u32SendCount += frb_count;
#endif 
    // interrupt process
    int times = 0;
    int first = true;
    //int counter = 0;
    int ret;
	fd_set rd_fds;
    struct f2sm_read_info_t read_info;

	while (1) {
		// 清空文件描述符集合
		FD_ZERO(&rd_fds);
		FD_SET(f2sm.m_fd, &rd_fds);

		// ret 待处理事件总数
		ret = select(the_fd+1, &rd_fds, NULL, NULL, NULL);
		if (ret > 0) {
			// 检测读管道文件描述符是否存储在tmp_fds中
			// 检测此文件描述符中是否有事件发生
			if (FD_ISSET(the_fd, &rd_fds)) 
            {
                //counter = 0;
                memset(&read_info,0,sizeof(read_info));
				int nRead = read(the_fd, &read_info, sizeof(read_info));

                if(nRead == sizeof(read_info))
                {
    				//printf("current event count  is: %d \n", counter);
                    printf("read info %d %d\n",read_info.irq_count, read_info.mem_index);

                    if(!first)
                    {
                        if(!f2sm.CheckResult(u32SendCount))   //check error
                        {
                            break;
                        }
                        times++;
                        if(times == TEST_TIMES)
                            break;

                    }
                    first = false;

                    pSrcPos = (char *)pDataBuf+offset;
                    //memcpy(pDestPos,pSrcPos,blk_size);
                    nSendByte = write(the_fd,pSrcPos,blk_size);
                    if(nSendByte != blk_size)
                    {
                        printf("write error! %d : %d \n", blk_size, nSendByte);
                        return;
                    }

                    if(offset == 0)
                    {
                        min_l = randv;
                        min_h = randv+1;
                    }
                    else
                    {
                        min_l = *(unsigned int *)(pSrcPos-8);
                        min_h = *(unsigned int *)(pSrcPos-4);      

                        /*
                        printf("V2-64 = 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n", 
                                *((long long *)(pSrcPos - 8)),
                                *((long long *)(pSrcPos + 0)),
                                *((long long *)(pSrcPos + 8)),
                                *((long long *)(pSrcPos + 16))); */
                    }    

                    if(u32SendCount > 0xFFFFFF00)
                    {
                        f2sm.ResetCounter();
                    }           

                    f2sm.StartTransfer(read_info.mem_index,min_l,min_h,frb_count);
                    //write(the_fd,&onv,sizeof(int));       // 开中断

                    offset += blk_size;                //already                
                    if(offset + blk_size > nLimit)     //will
                    {
                        offset = 0;
                    }

                    if(u32SendCount > 0xFFFFFF00)
                    {
                        u32SendCount = frb_count;
                    }
                    else
                    {
                        u32SendCount += frb_count;
                    }

                }
                else
                {
                    printf("Read error!\n");
                    break;
                }
			}
		}
	}

    gettimeofday(&tv_e,NULL);
    duration = (tv_e.tv_sec - tv_s.tv_sec)*1000 + (tv_e.tv_usec-tv_s.tv_usec)/1000;

    printf("current event count  is: %d \n", read_info.irq_count);
    printf("%d * %d B, used %lu ms\n",times, blk_size, duration);
    fflush(stdout); 
}

int f2sm_read_test(void)
{
    //f2sm_reg_test();
    //return 0;

    //printf("Start!\n");
    int randv = 0;
    int nByte = 30*1024*1024;
    int nLimit = 30*1020*1024;

    int nInt = nByte / 4;

    char *pDataBuf = (char *)aligned_alloc(4096,nByte); // 30MB    
    if(pDataBuf == NULL)
    {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }

    srand((unsigned)time(NULL));
    //randv = rand();  // 0~32767
    //PrepareData((int *)pDataBuf,nInt,randv);
    randv = (unsigned int)rand();  // 0~32767
    //rand_h = rand_l + 1;  // PRBS Multi-channel
    printf("Rand v: 0x%08x-%08x \n",randv+1,randv);

    unsigned int *pPrbsBuff = (unsigned int *) pDataBuf;
    GenPrbsArray(randv, 2, pPrbsBuff, nInt);

    //memset(pDataBuf,0,nByte); // for test only
    printf("PrepareData ready!\n");    
    printf("V2-64 = 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n", 
            *((long long *)(pDataBuf + 0)),
            *((long long *)(pDataBuf + 8)),
            *((long long *)(pDataBuf + 16)),
            *((long long *)(pDataBuf + 24)));

    CF2smIntf f2sm;
    int nBlock = 85;
    int rand_cnt_m,rand_cnt_a,rand_cnt_b;
    //char *pDestPos = (char *)f2sm.mem_info[0].mem_addr;

    while(1)
    {
    f2sm.Init();
        do_test(f2sm,1*85,pDataBuf,nLimit,randv);
    f2sm.Close();
    f2sm.Init();
        do_test(f2sm,1*80,pDataBuf,nLimit,randv);
    f2sm.Close();
    f2sm.Init();
        do_test(f2sm,1*90,pDataBuf,nLimit,randv);
    f2sm.Close();
    f2sm.Init();
        do_test(f2sm,2*85,pDataBuf,nLimit,randv);
    f2sm.Close();
    f2sm.Init();
        do_test(f2sm,3*85,pDataBuf,nLimit,randv);
    f2sm.Close();
    f2sm.Init();
        do_test(f2sm,4*85,pDataBuf,nLimit,randv);
    f2sm.Close();


    rand_cnt_m = rand()%5 + 1;
    rand_cnt_a = rand_cnt_m * 10;
    rand_cnt_b = rand()%9 + 1;
    nBlock = 85* rand_cnt_m + rand_cnt_a + rand_cnt_b;

    f2sm.Init();
        do_test(f2sm,nBlock,pDataBuf,nLimit,randv);
    f2sm.Close();

    }

    free(pDataBuf);

    return 0;
}

int main(void)
{
    //f2sm_memcpy_test();
    //f2sm_reg_test();
    f2sm_read_test();

    return 0;
}
