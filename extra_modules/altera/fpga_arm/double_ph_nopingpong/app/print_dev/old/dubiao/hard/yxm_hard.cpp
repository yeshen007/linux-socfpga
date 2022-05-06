#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>


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

#define PH2_USERBUF_OFFSET 	(16*1024*1024)

typedef unsigned char u8;
typedef unsigned int u32;


#if 0
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
#endif


static u8 linenum[16] = {15, 4, 9, 2, 12, 7, 10, 1, 14, 5, 8, 3, 13, 6, 11, 0};


/* 每个喷头block_cnt */
static int build_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
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
				*buf_ph1++ = (linenum[line] & 0xf) | (((u8)block << 4) & 0xf0);
			}
		}
	}
	
	return 0;

}

static int check_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
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
				if ((*buf_ph0 != row) || (*buf_ph1 != ((linenum[line] & 0xf) | (((u8)block << 4) & 0xf0)))) {
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
//	yxm.Step3();
	
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
	

	int the_fd = yxm.down_fd;
	struct f2sm_read_info_t read_info;
	
	memset(&read_info,0,sizeof(read_info));
	ret = read(the_fd, &read_info, sizeof(read_info));
	printf("first read must not wait\n");
	if (ret != sizeof(read_info)) {
		printf("first read error\n");
		return -1;
	}

	struct timeval tv_s;
    struct timeval tv_e;
	long duration = 0;
	gettimeofday(&tv_s,NULL);

	yxm.Step3_1();	

	sleep(1);

	/* 启动打印 */
	yxm.StartPrint();
	

	/* 等待正常结束打印或者异常结束打印 */
	ret = read(the_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("read error\n");
		return -1;
	} 

finish_print:	
	if (read_info.mem_index == 2) {
		printf("normal stop print\n");
		gettimeofday(&tv_e,NULL);
		duration = (tv_e.tv_sec - tv_s.tv_sec)*1000 + (tv_e.tv_usec - tv_s.tv_usec)/1000;
		printf("need time %lu ms\n", duration);
	} else if (read_info.mem_index == -2) {
		printf("accident stop print\n");
	} else {
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


