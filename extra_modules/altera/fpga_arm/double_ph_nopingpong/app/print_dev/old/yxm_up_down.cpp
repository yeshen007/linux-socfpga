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


#include "alt_f2sm_regs.h"
#include "YxmIntf.h"

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define TEST_TIMES 10

#define HPS_FPGA_BRIDGE_BASE 0xFF200000


#define TEST_BLOCK_SIZE 	(3*1024)
#define TEST_BLOCK_CNT 		1024
#define PH1_USERBUF_OFFSET 	(12*1024*1024)


#define _IOC_F2SM_START _IO(0x05, 0x01)
#define _IOC_F2SM_STOP  _IO(0x05, 0x02)
#define _IOC_F2SM_PAUSE _IO(0x05, 0x03)
#define _IOC_F2SM_DQBUF _IO(0x05, 0x04)

#define YESHEN_DEBUG(x) { printf("yeshen test %d\n", x); }

struct f2sm_read_info_t {
	u_int32_t irq_count;
	int mem_index;
};

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


static CYxmIntf yxm;


static int up_fd;
static int down_fd;

static u32 block_size = TEST_BLOCK_SIZE;
static u32 down_blk_size = block_size;
static u32 up_blk_size = block_size/3; //1kb
static u32 up_effect_blk_size = 768;	//768

static u8 *ph0_data; 
static u8 *ph1_data;
static u8 *up_buf; 

static int start_print = 0;

static u8 linenum[16] = {15, 4, 9, 2, 12, 7, 10, 1, 14, 5, 8, 3, 13, 6, 11, 0};


void *down_thread(void *arg);

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
				if ((*buf_ph0 != row) && (*buf_ph1 != (linenum[line] & 0xf) | (((u8)block << 4) & 0xf0))) {
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


#define LOOP_UP

int main(void)
{	
	int ret;
	u32 up_i;	//收到的第几个1kb	
	void *down_ret;
	pthread_t down_tid;

   	yxm.Init();
	up_fd = yxm.up_fd;
	down_fd = yxm.down_fd;
	
#ifdef LOOP_UP
	u32 up_blks_one_time = 16;
	up_buf = (u8 *)malloc(up_blk_size * 2 * up_blks_one_time); //2 ph each 1kb,so total 2kb
#endif

	ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	ph1_data = ph0_data + PH1_USERBUF_OFFSET;

	ret = build_fpga_data(ph0_data, ph1_data, block_size, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Build Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	
	ret = check_fpga_data(ph0_data, ph1_data, block_size, TEST_BLOCK_CNT);
	if (ret == -1) {
		fprintf(stderr, "Check Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}


	yxm.Step1();

#ifdef LOOP_UP	
	/* 启动一次上行传输 */
	yxm.StartTransfer_up(0, up_blks_one_time);
#endif

	yxm.Step2();

	yxm.Step3();


	/* 
	 * step4
	 */
	/* 下行子线程 */
	ret = pthread_create(&down_tid, NULL, down_thread, NULL); 
	if (ret != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_free_mem;
	}


	while (start_print != 1)
		usleep(5000);

	/* 开始打印 */	
	yxm.StartPrint();
	printf("start print\n");


#ifdef LOOP_UP
	/* 打印过程持续检测上行DMA中断。每次收到中断则启动下次一数据传输(配置数据块大小,地址6写1写0)
	 * 此过程软件不需要主动结束，有中断上来就一直配置传输
	 * 打印过程中持续检测硬件中断bit[5]和bit[4]。收到硬件中断bit[4]表示打印正常结束/收到硬件
	 * 中断bit[5]表示打印异常停止。打印输出寄存器内容,测试结束
	 */	
	for (up_i = 0; ; up_i++) {
		ret = read(up_fd, up_buf, up_blk_size * up_blks_one_time);
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
#endif

	
loop_release_down_thread:
	ret = pthread_join(down_tid, &down_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (down_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);

loop_release_free_mem:
#ifdef LOOP_UP
	free(up_buf);
#endif
	free(ph0_data);
	
	yxm.PrintRegs();
	yxm.FinishPrint();
	yxm.Close();	

	return ret;
}


void *down_thread(void *arg)
{
	int ret;
	int down_i;
	struct f2sm_read_info_t read_info;	
	u32 down_blks_one_time = 16;
	int m = 8000 * 16 / 4;

	//先发送一块dma
	ret = read(down_fd, &read_info, sizeof(read_info));
	if ((u32)ret != sizeof(read_info) || read_info.mem_index != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	ret = write(down_fd, ph0_data, down_blk_size * down_blks_one_time);
	if ((u32)ret != down_blk_size * down_blks_one_time) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	yxm.StartTransfer_down(1, down_blks_one_time);

	start_print = 1;

	//发送第二块到最后一块
	for (down_i = 1; down_i < m/down_blks_one_time; down_i++) {
		ret = read(down_fd, &read_info, sizeof(read_info));
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

		ret = write(down_fd, ph0_data, down_blk_size * down_blks_one_time);
		if ((u32)ret != down_blk_size * down_blks_one_time) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}
		yxm.StartTransfer_down(1, down_blks_one_time);
	}


	return (void *)1;
}



