#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#include <sys/time.h>

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

#define PH1_USERBUF_OFFSET 	(16*1024*1024)

typedef unsigned char u8;
typedef unsigned int u32;


static CYxmIntf yxm;


static int g_down_fd;


static u32 block_size = TEST_BLOCK_SIZE;	//3kb
static u32 down_blk_size = block_size;	//3kb
static u32 up_blk_size = block_size/3; //1kb


static u8 *g_ph0_base; 
static u8 *g_ph1_base;
static u8 *g_ph0_change; 
static u8 *g_ph1_change;



static u8 linenum[16] = {15, 4, 9, 2, 12, 7, 10, 1, 14, 5, 8, 3, 13, 6, 11, 0};


static int build_base(void *ph0_base, void *ph1_base, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_base;
	u8 *buf_ph1 = (u8 *)ph1_base;

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
				*buf_ph1++ = 0;
			}
		}
	}
	
	return 0;

}

static int build_change(void *ph0_change, void *ph1_change, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_change;
	u8 *buf_ph1 = (u8 *)ph1_change;

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
				*buf_ph0++ = 0; 
				*buf_ph1++ = (linenum[line] & 0xf) | (((u8)block << 4) & 0xf0);
			}
		}
	}
	
	return 0;

}


void base_call(void)
{
	int ret;
	struct f2sm_read_info_t read_info;	
	u32 down_i;
	u32 down_blks_one_time = 16;
	u32 total = TEST_BLOCK_SIZE * TEST_BLOCK_CNT;


	//发送第一块到最后一块
	for (down_i = 0; down_i < total / (down_blks_one_time * down_blk_size); down_i++) {
		ret = read(g_down_fd, &read_info, sizeof(read_info));
		if ((u32)ret != sizeof(read_info)) {	//为了和up保持代码一致，硬件不出问题不会在下行期间来结束打印中断
			if (ret == 1) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return ;
			} else if (ret ==2) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return ;
			} else {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return ;
			}
		}	
		if (read_info.mem_index != 0) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return ;
		}

		ret = write(g_down_fd, g_ph0_base, down_blk_size * down_blks_one_time);
		if ((u32)ret != down_blk_size * down_blks_one_time) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return ;
		}
		yxm.StartTransfer_down(1, down_blks_one_time);
	}


}



int main(void)
{

	int ret;

	yxm.Init();
	g_down_fd = yxm.down_fd;

	g_ph0_base = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	g_ph1_base = g_ph0_base + PH1_USERBUF_OFFSET;
	g_ph0_change = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	g_ph1_change = g_ph0_change + PH1_USERBUF_OFFSET;


	unsigned int dma_blks_one_time = 16;
	unsigned int sended_blks_cnt = 0;
	struct f2sm_read_info_t read_info;
	unsigned int times;
	unsigned int total = TEST_BLOCK_SIZE * TEST_BLOCK_CNT;		

	ret = build_base(g_ph0_base, g_ph1_base, block_size, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Build Base Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ret = build_change(g_ph0_change, g_ph1_change, block_size, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Build Change Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}	
	
	
	/* step 0 */
	yxm.ConfigMaster();

	
	/* 
	 * step1
	 */
	yxm.PrintDi();
	
	/* 
	 * step2
	 */
	yxm.ConfigRasterSim();
	yxm.ConfigPdSim();
	
	/*
	 * step3
	 */
	yxm.ConfigDivosr();
	yxm.ConfigJob();
	yxm.ConfigOffset();
	yxm.ConfigFire();
	yxm.ConfigSwitch();	
	yxm.ConfigLabel();	

	/*
	 * step4
	 */
	yxm.TestDataEn();


	/* 
	 * step5
	 */	
	yxm.BaseImageEn();

	base_call();

	yxm.WaitBaseImageTranferComplete();


	yxm.BaseImageDi();

	
	 
	/* 启动DMA，发送第1块缓存数据。第一次收到DMA中断信号后，暂停发送数据 */
	memset(&read_info,0,sizeof(read_info));
	ret = read(g_down_fd, &read_info, sizeof(read_info));
	printf("first read must not wait\n");
	if (ret != sizeof(read_info)) {
		printf("first read error\n");
		return -1;
	}
	
	ret = write(g_down_fd, g_ph0_change, dma_blks_one_time * down_blk_size);
	if (ret != (int)(dma_blks_one_time * down_blk_size)) {
		printf("first write error\n");
		return -1;		
	}
	
	yxm.StartTransfer_down(1, dma_blks_one_time);
	
	memset(&read_info,0,sizeof(read_info));
	ret = read(g_down_fd, &read_info, sizeof(read_info));
	if (ret != sizeof(read_info)) {
		printf("second read error\n");
		return -1;
	}
	
	printf("first dma interrupt reav\n");
	sended_blks_cnt += dma_blks_one_time;

	/* 启动打印 */
	yxm.PrintEn();

	/* 开始硬件仿真 */
	yxm.SyncSimEn();


	/* 
	 * step6
	 */		

	/* 继续DMA发送，每次收到中断都启动下一次发送，直到数据全部发送完成则不响应中断. 
     * 打印过程中持续检测硬件中断bit[4]和bit[5].
     * 收到硬件中断bit[4]表示打印正常结束/收到硬件中断bit[5]表示打印异常停止.
     * 打印输出寄存器内容,测试结束.
	 */
	for (times = 1; times < total / (dma_blks_one_time * down_blk_size); times++) {
		ret = write(g_down_fd, g_ph0_change, dma_blks_one_time * down_blk_size);
		if (ret != (int)(dma_blks_one_time * down_blk_size)) {
			printf("write error\n");
			return -1;		
		}
		
		yxm.StartTransfer_down(1, dma_blks_one_time);
		
		memset(&read_info,0,sizeof(read_info));
		ret = read(g_down_fd, &read_info, sizeof(read_info));
		if (ret != sizeof(read_info)) {
			printf("get print interrupt or read error\n");
			goto finish_print;
		}	

	}

	/* 等待正常结束打印或者异常结束打印 */
	ret = read(g_down_fd, &read_info, sizeof(read_info));
finish_print:	
	if (ret == 1) 
		printf("down read normal stop\n");
	else if (ret ==2) 
		printf("down read accident stop\n");
	else 
		printf("down read wrong\n");	
	 

	/* 
     * 打印寄存器
     * 结束打印
	 */
	yxm.PrintRegs();

	/* 
	 * step7
	 */	
	yxm.PrintDi();
	

	/* 释放数据 */
	free(g_ph0_base);
	free(g_ph0_change);

	yxm.Close();

	return ret;
}




