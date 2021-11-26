
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

#define MIN_START_VALUE 1
#define TEST_TIMES 300
//#define TEST_TIMES 30
#define CLK_REP_TESTS 1000 //repetitions to get clock statistics

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

void do_test(CF2smIntf &f2sm, char *pDataBuf)
{
    int frb_size = 12*1024;
    int frb_count = 85;

    int blk_size = frb_size * frb_count;       // 85 * 12K
    int offset = 0;

    int the_fd = f2sm.m_fd;

    //Start
    int rand_l;
    int rand_h;
    char *pSrcPos = NULL;

    rand_h = rand();  // 0~32767
    rand_l = rand()%0xffff;  // 0~32767
    printf("Rand v: 0x%08x-%08x \n",rand_h,rand_l);

    f2sm.StartTransfer(rand_l,rand_h,frb_count);

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
                        //overflow = pmu_counter_read_ns(&pmu_counter_ns);
                        //if (overflow == 1){printf("PMU Counter Cycle counter overflow!! \n");return;}

                        //pmu_counter_sum += pmu_counter_ns;
                        //pmu_counter_reset();
                    //printf("read info %d %d\n",read_info.irq_count, read_info.mem_index);

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
                        min_l = MIN_START_VALUE-1;
                        min_h = randv;
                    }
                    else
                    {
                        min_l = *(int *)(pSrcPos-8);
                        min_h = randv;      

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

    printf("%d * %d B\n",times, blk_size);
    fflush(stdout); 
}

int f2sm_write_test(void)
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