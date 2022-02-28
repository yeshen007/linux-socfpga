#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "alt_f2sm_regs.h"
#include "YxmIntf.h"

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define TEST_TIMES 10

#define HPS_FPGA_BRIDGE_BASE 0xFF200000

struct f2sm_read_info_t {
	u_int32_t irq_count;
	int mem_index;
};

#define TEST_BLOCK_SIZE (3*1024)
#define TEST_BLOCK_CNT 1024

#define PH2_USERBUF_OFFSET 	(12*1024*1024)

typedef unsigned char u8;
typedef unsigned int u32;



/* 每个喷头block_cnt */
int build_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_data;
	u8 *buf_ph1 = (u8 *)ph1_data;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}

	/* build data */
	u8 row;
	u8 line;
	u32 block;
	for (block = 0; block < block_cnt; block++) {
		for (row = 0; row < 192; row++) {
			for (line = 0; line < 16; line++) {
				*buf_ph0++ = row; 
				*buf_ph1++ = (line & 0xf) | (((u8)block << 4) & 0xf0);
			}
		}
	}
	
	return 0;
}

int check_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_data;
	u8 *buf_ph1 = (u8 *)ph1_data;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}

	/* check data */
	u8 row;
	u8 line;
	u32 block;
	for (block = 0; block < block_cnt; block++) {
		for (row = 0; row < 192; row++) {
			for (line = 0; line < 16; line++) {
				if ((*buf_ph0 != row) && (*buf_ph1 != (line & 0xf) | (((u8)block << 4) & 0xf0))) {
					fprintf(stderr, "check data Error at line %d, file %s\n", __LINE__, __FILE__);
					return -1;
				}
				buf_ph0++; 
				buf_ph1++;
			}
		}
	}	

	return 0;
}


int main(void)
{
	
	CYxmIntf yxm;

   	yxm.Init();
	
	/* 
	 * step1
	 */
	yxm.Step1();
	
	/* 
	 * step2
	 */
	yxm.Step2();
	
	/*
	 * step3
	 */
	yxm.Step3();
	
	/* 
	 * step4
	 */
	/* 构造和预先检查数据 */
	int ret;
	u32 block_size = TEST_BLOCK_SIZE;
	u32 block_cnt = TEST_BLOCK_CNT;	
	u8 *ph0_data = (u8 *)malloc(2*PH2_USERBUF_OFFSET);
	u8 *ph1_data = ph0_data + PH2_USERBUF_OFFSET;

	ret = build_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Build Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	
	ret = check_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Check Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}	
	
	/* 启动DMA，发送第1块缓存数据。第一次收到DMA中断信号后，暂停发送数据 */
	unsigned int dma_blks_one_time = 64;
	unsigned int sended_blks_cnt = 0;
	int the_fd = yxm.m_fd;
	struct f2sm_read_info_t read_info;
	
	memset(&read_info,0,sizeof(read_info));
	ret = read(the_fd, &read_info, sizeof(read_info));
	printf("first read must not wait\n");
	if (ret != sizeof(read_info)) {
		printf("first read error\n");
		return -1;
	}
	
	ret = write(the_fd, ph0_data + sended_blks_cnt * block_size, dma_blks_one_time * block_size);
	if (ret != (int)(dma_blks_one_time * block_size)) {
		printf("first write error\n");
		return -1;		
	}
	yxm.StartTransfer(read_info.mem_index, 66, 99, dma_blks_one_time);
	
	memset(&read_info,0,sizeof(read_info));
	ret = read(the_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("second read error\n");
		return -1;
	}
	
	printf("first dma interrupt reav\n");
	sended_blks_cnt += dma_blks_one_time;

	/* 启动打印 */
	yxm.StartPrint();
	

	/* 继续DMA发送，每次收到中断都启动下一次发送，直到数据全部发送完成则不响应中断. 
     * 打印过程中持续检测硬件中断bit[4]和bit[5].
     * 收到硬件中断bit[4]表示打印正常结束/收到硬件中断bit[5]表示打印异常停止.
     * 打印输出寄存器内容,测试结束.
	 */
	for ( ; sended_blks_cnt < block_cnt; sended_blks_cnt += dma_blks_one_time) {
		ret = write(the_fd, ph0_data + sended_blks_cnt * block_size, dma_blks_one_time * block_size);
		if (ret != (int)(dma_blks_one_time * block_size)) {
			printf("write error\n");
			return -1;		
		}
		yxm.StartTransfer(read_info.mem_index, 66, 99, dma_blks_one_time);
		
		memset(&read_info,0,sizeof(read_info));
		ret = read(the_fd, &read_info, sizeof(read_info));
		if (ret != sizeof(read_info)) {
			printf("read error\n");
			return -1;
		}	
		if (read_info.mem_index == 2 || read_info.mem_index == -2) 
			goto finish_print;
	}

	/* 等待正常结束打印或者异常结束打印 */
	ret = read(the_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("read error\n");
		return -1;
	}

finish_print:	
	if (read_info.mem_index == 2)
		printf("normal stop print\n");
	else if (read_info.mem_index == -2)
		printf("accident stop print\n");
	else {
		printf("read error,no stop print\n");
		return -1;
	}
	 

	/* 
     * 打印寄存器
     * 结束打印
	 */
	yxm.PrintRegs();
	yxm.FinishPrint();
	

	/* 释放数据 */
	free(ph0_data);

	yxm.Close();

	return 0;
}

