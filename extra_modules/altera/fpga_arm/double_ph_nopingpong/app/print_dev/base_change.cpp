
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sched.h>

#include "alt_f2sm_regs.h"
#include "YxmIntf.h"


#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define TEST_TIMES 50

#define HPS_FPGA_BRIDGE_BASE 0xFF200000


#define TEST_BLOCK_SIZE (3*1024)
#define TEST_BLOCK_CNT 1024

#define PH1_USERBUF_OFFSET 	(16*1024*1024)


#define YESHEN_DEBUG(x) { printf("yeshen test %d\n", x); }

struct f2sm_read_info_t {
	u_int32_t irq_count;
	int mem_index;
};

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


static CYxmIntf yxm;


static int g_up_fd;
static int g_down_fd;



static u32 block_size = TEST_BLOCK_SIZE;	//3kb
static u32 down_blk_size = block_size;	//3kb
static u32 up_blk_size = block_size/3; //1kb


static u8 *g_ph0_base; 
static u8 *g_ph1_base;
static u8 *g_ph0_change; 
static u8 *g_ph1_change;


static u8 *g_up_buf; 


static int start_print = 0;



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


void base_call(void);
void *change_thread(void *arg);


int main(void)
{	
	int ret;
	u32 up_i;	//收到的第几个1kb	
	void *change_ret;
	pthread_t change_tid;
	u32 up_blks_one_time = 16;
	unsigned int hole_addr;

   	yxm.Init();
	g_up_fd = yxm.up_fd;
	g_down_fd = yxm.down_fd;

	g_up_buf = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);

	g_ph0_base = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	g_ph1_base = g_ph0_base + PH1_USERBUF_OFFSET;
	g_ph0_change = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	g_ph1_change = g_ph0_change + PH1_USERBUF_OFFSET;

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

	yxm.InitHole();

	for (hole_addr = 0; hole_addr < 332; hole_addr++) {
		yxm.WriteHole(0, hole_addr, 0xff00);
		yxm.WriteHole(1, hole_addr, 0xcc33);
	}


	yxm.Step1();

	yxm.StartTransfer_up(0, up_blks_one_time);

	yxm.Step2();

	yxm.Step3();	

	yxm.EnableBase();

	base_call();

	yxm.PollTest();

	yxm.CloseBase();

	ret = pthread_create(&change_tid, NULL, change_thread, NULL); 
	if (ret != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_free_mem;
	}

	while (start_print != 1)
		usleep(5000);

	yxm.StartPrint();
	printf("start print\n");

	for (up_i = 0; ; up_i++) {
		ret = read(g_up_fd, g_up_buf, up_blk_size * up_blks_one_time);
		if ((u32)ret != up_blk_size * up_blks_one_time) {			//打印结束中断
			if (ret == 1) {
				printf("up read normal stop, index %d\n", up_i);			
				goto loop_release_down_thread;
			} else if (ret == 2) {
				printf("up read accident stop, index %d\n", up_i);
				goto loop_release_down_thread;
			} else {
				printf("up read wrong, index %d\n", up_i);
				goto loop_release_down_thread;
			}
		}

		yxm.StartTransfer_up(0, up_blks_one_time);
	}	

loop_release_down_thread:
	ret = pthread_join(change_tid, &change_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (change_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);

loop_release_free_mem:
	free(g_ph0_change);
	free(g_ph0_base);
	free(g_up_buf);

	yxm.PullDown_52();
	sleep(1);
	yxm.PrintRegs();
	sleep(1);
	
	yxm.FinishPrint();
	yxm.Close();	

	return ret;	

}


void base_call(void)
{
	int ret;
	struct f2sm_read_info_t read_info;	
	u32 down_i;
	u32 down_blks_one_time = 16;
	u32 m = 262144 / 4;


	//发送第一块到最后一块
	for (down_i = 0; down_i < m/down_blks_one_time; down_i++) {
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


void *change_thread(void *arg)
{
	int ret;
	struct f2sm_read_info_t read_info;	
	u32 down_i;
	u32 down_blks_one_time = 16;
	u32 m = 262144 / 4;

	//先发送一块dma
	ret = read(g_down_fd, &read_info, sizeof(read_info));
	if ((u32)ret != sizeof(read_info) || read_info.mem_index != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	ret = write(g_down_fd, g_ph0_change, down_blk_size * down_blks_one_time);
	if ((u32)ret != down_blk_size * down_blks_one_time) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	yxm.StartTransfer_down(1, down_blks_one_time);

	start_print = 1;

	//发送第二块到最后一块
	for (down_i = 1; down_i < m/down_blks_one_time; down_i++) {
		ret = read(g_down_fd, &read_info, sizeof(read_info));
		if ((u32)ret != sizeof(read_info)) {	//为了和up保持代码一致，硬件不出问题不会在下行期间来结束打印中断
			if (ret == 1) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return NULL;
			} else if (ret ==2) {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return NULL;
			} else {
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
				return NULL;
			}
		}	
		if (read_info.mem_index != 0) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}

		ret = write(g_down_fd, g_ph0_change, down_blk_size * down_blks_one_time);
		if ((u32)ret != down_blk_size * down_blks_one_time) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}
		yxm.StartTransfer_down(1, down_blks_one_time);
	}


	return (void *)1;

}




