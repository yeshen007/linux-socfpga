
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "alt_f2sm_regs.h"

#include "F2smIntf.h"
#include "utils.h"

#define MIN_START_VALUE 1
#define TEST_TIMES 300
#define CLK_REP_TESTS 1000 //repetitions to get clock statistics

struct f2sm_read_info_t
{
	u_int32_t irq_count;
	int mem_index;
};


#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define DMA_BUFFER_SIZE 0x600000

void prefetch_send(CF2smIntf &f2sm)
{
	
    int randv = 0;
	int randv1 = 0;
    int nByte = 6*1024*1024;	//hps申请的内存字节数
    int nInt = nByte / 4;		//内存字数
    int blkcount = nByte / (4*4096);

	int the_fd = f2sm.m_fd;
	int nRead;
	int nSendByte;
	struct f2sm_read_info_t read_info;
    memset(&read_info, 0, sizeof(read_info));
	
	/* 申请6mb的4kb对齐的内存 */
    char *pDataBuf = (char *)aligned_alloc(3*4096,nByte);     
    if (pDataBuf == NULL) {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }

    srand((unsigned)time(NULL));
    randv = (unsigned int)rand();  // 0~32767
    printf("Rand v: 0x%08x-%08x \n",randv+1,randv);

    unsigned int *pPrbsBuff = (unsigned int *)pDataBuf;	
	/* 产生随机数列并写入用户buffer中 */
    GenPrbsArray(randv, 2, pPrbsBuff, nInt);

	/* 填满两个dma     buffer，两个dma buffer填一样的数据，这样randv就好处理 */
	nRead = read(the_fd, &read_info, sizeof(read_info));
	if (unlikely(nRead != sizeof(read_info))) {
		printf("preRead error!\n");
		return;
	}
	nSendByte = write(the_fd, pPrbsBuff, nByte);	
	if (unlikely(nSendByte != nByte)) {
		printf("prewrite error!\n");
		return;
	}
	nRead = read(the_fd, &read_info, sizeof(read_info));
	if (unlikely(nRead != sizeof(read_info))) {
		printf("preRead error!\n");
		return;
	}
	nSendByte = write(the_fd, pPrbsBuff, nByte);	
	if (unlikely(nSendByte != nByte)) {
		printf("prewrite error!\n");
		return;
	}

	free(pDataBuf);
	
	/* 启动dma传输 */
	f2sm.StartTransfer(0, randv, randv + 1, blkcount);
	f2sm.StartTransfer(1, randv, randv + 1, blkcount);
	
}

/* 
 * frb_count是一次write传入的数据
 */
void do_pingpong_test(CF2smIntf &f2sm, int frb_count, char *pDataBuf,int nLimit,int randv)
{
    int frb_size = 12*1024;
	/* 一次dma传送的数据块大小，但是往fpga寄存器写入的是frb_count */
    int blk_size = frb_size * frb_count;       // 85 * 12K
    int new_blk_size = blk_size;
	int old_blk_size = blk_size;
	int new_frb_count = frb_count;
	int blk_size_reset = 0;

    struct timeval tv_s;
    struct timeval tv_e;
    long duration = 0;

	/* 驱动文件描述符 */
    int the_fd = f2sm.m_fd;
    f2sm.ResetCounter();	

    //Start
    unsigned int min_l;
    unsigned int min_h;
    u_int32_t u32SendCount = 0;	//累计传送的字节数，可清零
    char *pSrcPos = NULL;	//某次dma传送起始地址
    int nSendByte = 0;	//某次dma传送的字节数
	int offset = 0;		//某次dma传送起始地址与用户数据总基地址pDataBuf的偏移
	int newoffset = 0;
	int cnt = 0;
    gettimeofday(&tv_s,NULL);


    // interrupt process
    int times = 0;
    int first = true;
    int ret;

	/* 初始化read_info */
	struct f2sm_read_info_t read_info;
    memset(&read_info, 0, sizeof(read_info));

	/* 预先使用另外的种子和随机数将两个dma buffer填满发送 */
	prefetch_send(f2sm);	

	f2sm.ResetCounter();

	/* 发送5次大概50mb的随机数据 */
	while (cnt < 5) {

		/* 如果本次发送超过50mb上届则偏移回零，并增加一次50mb计数 */
        if (offset + blk_size > nLimit) {     
       		offset = 0;
			cnt++;
        }

		/* 读取队列中第一个空闲的dma buffer，如果没有则睡眠等待
         * 因为有了prefetch_send()这里有可能会睡眠
		 */
		int nRead = read(the_fd, &read_info, sizeof(read_info));
		if (unlikely(nRead != sizeof(read_info))) {
			printf("Read error!\n");
			break;
		}

		/* 获取本次dma传输的起始地址
         * 校验本次传输数据量大小blk_size是否符合12kb的整数倍
		 */
        pSrcPos = (char *)pDataBuf + offset;	
        if (unlikely((blk_size % frb_size) != 0)) {
			printf("wrong blk size!\n");
			break;
        }

		/* 将本次dma传输的数据从用户空间拷贝到dma bufer
         * 但超过DMA_BUFFER_SIZE的部分要到下一次循环再发，
         * 因为f2sm.StartTransfer一次只能启动一个dma buffer传输
		 */
        if (unlikely(blk_size < 0)) {
			printf("wrong write count1!\n");
			return;			
		} else if (blk_size <= DMA_BUFFER_SIZE) {
			nSendByte = write(the_fd, pSrcPos, blk_size);	//本次实际写入dma buffer的数据大小
			if (unlikely(nSendByte != blk_size)) {
				printf("write error!\n");
				return;
			}
			newoffset = offset + nSendByte;					//下一次传输数据的偏移offset
			new_blk_size = blk_size;						//下一次传输预期写入dma buffer的数据大小
			if (blk_size_reset == 1) {
				blk_size_reset = 0;
				new_blk_size = old_blk_size;
			}
        } else if (blk_size > DMA_BUFFER_SIZE ) {
			nSendByte = write(the_fd, pSrcPos, DMA_BUFFER_SIZE);
			if (unlikely(nSendByte != DMA_BUFFER_SIZE)) {
				printf("write error!\n");
				return;
			}			
			newoffset = offset + nSendByte;
			/* 下一次要写剩下的，那下下次怎么回来
			 * 通过blk_size_reset
			 */
			new_blk_size = blk_size - DMA_BUFFER_SIZE;		
			if (new_blk_size <= DMA_BUFFER_SIZE) 
				blk_size_reset = 1;
		} 
    
        /* 根据offset设置传给fpga的随机数种子 */
	    if (offset == 0) {
            min_l = randv;
            min_h = randv+1;
        } else {
            min_l = *(unsigned int *)(pSrcPos-8);
            min_h = *(unsigned int *)(pSrcPos-4);      
        }  

		/* 根据本轮实际写入dma buffer的数据大小nSendByte设置块数 */
		new_frb_count = nSendByte / frb_size;

		/* 启动dma传输 */
        f2sm.StartTransfer(read_info.mem_index, min_l, new_frb_count);	
		if (u32SendCount > 0xFFFFFF00) {
            f2sm.ResetCounter();
            u32SendCount = new_frb_count;
        } else
            u32SendCount += new_frb_count;		

		/* 更新下一次dma传输地址偏移和传输数据块的大小 */
		offset = newoffset;                             	
		blk_size = new_blk_size;		
	}

    gettimeofday(&tv_e,NULL);
    duration = (tv_e.tv_sec - tv_s.tv_sec)*1000 + (tv_e.tv_usec-tv_s.tv_usec)/1000;

    printf("current event count  is: %d \n", read_info.irq_count);
    printf("%d * %d B, used %lu ms\n",times, blk_size, duration);
    fflush(stdout); 
}

/* 三种测试情况
 * 1.调用do_pingpong_test传入的数据小于等于8mb(一个dma buffer)
 * 2.调用do_pingpong_test传入的数据大于8mb小于等于16mb
 * 3.调用do_pingpong_test传入的数据大于16mb
 */
int pingpong_test(void)
{
    CF2smIntf f2sm;		
    int randv = 0;
    int nByte = 60*1024*1024;	//hps申请的内存字节数
    int nLimit = 50*1024*1024;	//等于nByte
    int nInt = nByte / 4;	//内存字数

	/* 1.1) hps端申请30mb的4kb对齐的内存 */
    char *pDataBuf = (char *)aligned_alloc(4096,nByte); // 30MB    
    if(pDataBuf == NULL) {
        fprintf(stderr, "alloc buf for data failed!\n");
        exit(-1);        
    }

    srand((unsigned)time(NULL));
    randv = (unsigned int)rand();  // 0~32767
    printf("Rand v: 0x%08x-%08x \n",randv+1,randv);

    unsigned int *pPrbsBuff = (unsigned int *) pDataBuf;	
	/* 1.2) 产生随机数列并写入用户buffer中 */
    GenPrbsArray(randv, 2, pPrbsBuff, nInt);

 	
	/* 
	 * 获取文件/dev/fpgar_ram的文件描述符m_fd 
	 * 映射h2f桥基地址到用户空间的virtual_base
	 * 填充mem_info
     */
	f2sm.Init();	
	 
	/* 循环2) 3) 4) 5) 6) */
    do_pingpong_test(f2sm,90,pDataBuf,nLimit,randv);
	/* 释放m_fd和virtual_base */

	f2sm.Close();


    free(pDataBuf);

    return 0;
}

int main(void)
{

	/* fpga读取hps ddr测试 */
    pingpong_test();

    return 0;
}
