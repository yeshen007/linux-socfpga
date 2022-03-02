
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
#define TEST_TIMES 3000000
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

#if 0

void do_test(CF2smIntf &f2sm, char *pDataBuf)
{
    int frb_size = 3*1024;
    int frb_count = 1024;
    int blk_size = frb_size * frb_count;       // 3M
    int nInt =  blk_size/4;
	int nByte = 8*1024*1024;  // max,要大于3M
	
    int the_fd = f2sm.m_fd;

    /* 产生种子 */
    int seed;
    seed = (unsigned int)rand();  // 0~32767
    printf("Rand v: 0x%08x\n",seed);

    /* pPRBSBuf为软件产生随机数地址，pDataBuf为接收的硬件随机数地址 */ 
    unsigned int *pPRBSBuf = (unsigned int *)aligned_alloc(4096, nByte);   
    if(pPRBSBuf == NULL)
    {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }

	/* 软件产生随机数 */
    GenPrbsArray(seed, 4, pPRBSBuf, nInt);
//    GenPrbsArray_fix(seed, 4, pPRBSBuf, nInt);

    // interrupt process
    int times = 0;
    //int counter = 0;
    int ret;
    fd_set rd_fds;

    
    f2sm.ResetCounter();
	/* 告诉fpga种子，hps dma地址和要写的大小，然后启动 */
    f2sm.StartTransfer(seed, frb_count);


	while (1) {
        printf("yeshen test1\n");
		FD_ZERO(&rd_fds);
		FD_SET(f2sm.m_fd, &rd_fds);

        memset(pDataBuf,0,blk_size);
		ret = select(the_fd+1, &rd_fds, NULL, NULL, NULL);
		if (ret > 0) {
            printf("yeshen test2\n");
			if (FD_ISSET(the_fd, &rd_fds)) 
            {
                printf("yeshen test3\n");
                /* 把fpga写到hps ddr(物理地址0x31000000和0x31800000)的数据读到pDataBuf */
				int nRead = read(the_fd, pDataBuf, blk_size);
                printf("yeshen test4\n");
                if(nRead == blk_size)
                {
                	/* 将fpga的数据和软件的数据比较 */
                    printf("input value (0x%08x)\n", (unsigned int)frb_count);
                    if(!f2sm.CheckResult(seed, pDataBuf, (char *)pPRBSBuf, (unsigned int)frb_count))   //check error
                    {
                        break;
                    }
					else
					{
						printf("CheckResult ok\n");
                        f2sm.ResetCounter();
					}	
					
                    times++;
                    if(times == TEST_TIMES)
                        break;
                   // usleep(30*1000);

                    seed = rand();  // 0~32767

                    //fflush(stdout);

                    GenPrbsArray(seed, 4, pPRBSBuf, nInt);
                    //GenPrbsArray_fix(seed, 4, pPRBSBuf, nInt);

					/* 告诉fpga按照seed生成数据写到hps ddr */
                    f2sm.StartTransfer(seed, frb_count);
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

    srand((unsigned)time(NULL));
    int nByte = 30*1024*1024;
    char *pDataBuf = (char *)aligned_alloc(4096,nByte); // 30MB    
    if(pDataBuf == NULL)
    {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }


    CF2smIntf f2sm;
    f2sm.Init();

    do_test(f2sm, pDataBuf);

    f2sm.Close();

    free(pDataBuf);

    return 0;
}

int main(void)
{
    f2sm_write_test();

    return 0;
}
#endif

void fix_do_test(CF2smIntf &f2sm, char *pDataBuf)
{
    int frb_size = 3*1024;
    int frb_count = 1024;
	int onekb_frb_count = 1024 * 3;
    int blk_size = frb_size * frb_count;       // 3M
    int nInt =  blk_size/4;
	int nByte = 8*1024*1024;  // max,要大于3M	
    int the_fd = f2sm.up_fd;

    f2sm.testinit();

    /* 产生种子 */
    unsigned int seed;
	seed = (unsigned int)rand();
    printf("Rand v: 0x%08x\n",seed);

    /* pPRBSBuf为软件产生随机数地址，pDataBuf为接收的硬件随机数地址 */ 
    unsigned int *pPRBSBuf = (unsigned int *)aligned_alloc(4096, nByte);   
    if(pPRBSBuf == NULL) {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }

	/* 软件产生随机数 */
    GenPrbsArray(seed, 4, pPRBSBuf, nInt);

    int times = 0;

	f2sm.ResetCounter();
	/* 告诉fpga种子，hps dma地址和要写的大小，然后启动 */
    f2sm.StartTransfer_up_seed(seed, onekb_frb_count);


	while (1) {
        memset(pDataBuf, 0, blk_size);			
        /* 把fpga写到hps ddr(物理地址0x31000000和0x31800000)的数据读到pDataBuf */
		int nRead = read(the_fd, pDataBuf, blk_size);
        if (nRead == blk_size) {
        	/* 将fpga的数据和软件的数据比较 */
            if(!f2sm.CheckResult(seed, pDataBuf, (char *)pPRBSBuf, (unsigned int)onekb_frb_count)) {
				printf("CheckResult error\n");
                return;
			} else {
				printf("CheckResult ok\n");
				f2sm.ResetCounter();
			}	
			
            times++;
            if(times == TEST_TIMES) {
				printf("test done\n");
                break;
            }

			seed = (unsigned int)rand();
			printf("Rand v: 0x%08x\n",seed);

            GenPrbsArray(seed, 4, pPRBSBuf, nInt);
   
			/* 告诉fpga按照seed生成数据写到hps ddr */
            f2sm.StartTransfer_up_seed(seed, onekb_frb_count);
        }else {
            printf("Read error!\n");
            break;
        }
		
	}

    printf("%d * %d B\n", times, blk_size);
    fflush(stdout); 

    free(pPRBSBuf);
}

int fix_f2sm_write_test(void)
{
    srand((unsigned)time(NULL));
    int nByte = 30*1024*1024;
    char *pDataBuf = (char *)aligned_alloc(4096,nByte); // 30MB
    if(pDataBuf == NULL)
    {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);
    }

    CF2smIntf f2sm;
    f2sm.Init();

    fix_do_test(f2sm, pDataBuf);

    f2sm.Close();

    free(pDataBuf);

    return 0;
}

int main(void)
{
    fix_f2sm_write_test();

    return 0;
}
