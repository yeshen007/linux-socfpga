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

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;



static CYxmIntf yxm;


static int up_fd;
static int down_fd;

static u32 block_size = TEST_BLOCK_SIZE;
static u32 down_blk_size = block_size;


static u8 *ph0_data; 
static u8 *ph1_data;

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
				if ((*buf_ph0 != row) && (*buf_ph1 != ((linenum[line] & 0xf) | (((u8)block << 4) & 0xf0)))) {
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
	int ret;
	void *down_ret;
	pthread_t down_tid;

   	yxm.Init();
	up_fd = yxm.up_fd;
	down_fd = yxm.down_fd;
	

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


	yxm.Step2();

	yxm.Step3();


	/* 
	 * step4
	 */
	/* 下行线程 */
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

	ret = pthread_join(down_tid, &down_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (down_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);

loop_release_free_mem:
	free(ph0_data);

	yxm.PullDown_52();
	sleep(1);
	yxm.PrintRegs();
	sleep(1);
	
	yxm.FinishPrint();
	yxm.Close();	

	return ret;
}


void *down_thread(void *arg)
{
	int ret;
	struct f2sm_read_info_t read_info;	
	u64 down_blks_one_time = 16;
	u64 jobs = 160000;
	u64 m = 40000ULL * jobs / 4ULL;
	u64 down_i;

	FILE *log_tmp_file;	
	struct timeval tv_s;
    struct timeval tv_e;
	long duration = 0;
	log_tmp_file = fopen("/root/log-tmp.txt", "w+");
	if (log_tmp_file == NULL)
		fprintf(stderr, "open log-tmp.txt error\n");
	gettimeofday(&tv_s,NULL);

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


	//准备第二块的dma
	ret = read(down_fd, &read_info, sizeof(read_info));
	if ((u32)ret != sizeof(read_info) || read_info.mem_index != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}

	//发送第二块到最后一块
	for (down_i = 1; down_i < m/down_blks_one_time; down_i++) {
		ret = write(down_fd, ph0_data, down_blk_size * down_blks_one_time);
		if ((u32)ret != down_blk_size * down_blks_one_time) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}
		yxm.StartTransfer_down(1, down_blks_one_time);
		
		ret = read(down_fd, &read_info, sizeof(read_info));
		if ((u32)ret != sizeof(read_info)) {	//为了和up保持代码一致，硬件不出问题不会在下行期间来结束打印中断
			if (ret == 1) {
				printf("down read normal stop, index %llu\n", down_i);
				return (void *)1;
			} else if (ret ==2) {
				printf("down read accident stop, index %llu\n", down_i);
				return NULL;
			} else {
				printf("down read wrong, index %llu\n", down_i);
				return NULL;
			}
		}	
		if (read_info.mem_index != 0) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}

	}

	
	gettimeofday(&tv_e,NULL);
	duration = tv_e.tv_sec - tv_s.tv_sec;
	printf("need time %lu s\n", duration);	
	fprintf(log_tmp_file, "down_i: %llu, duration: %ld\n", down_i, duration);
	fclose(log_tmp_file);


	/* 等待正常结束打印或者异常结束打印 */
	ret = read(down_fd, &read_info, sizeof(read_info));
	if (ret == 1) {
		printf("down read normal stop\n");
		return (void *)1;
	} else if (ret ==2) {
		printf("down read accident stop\n");
		return NULL;
	} else {
		printf("down read wrong\n");
		return NULL;
	}	
	
	return (void *)1;
}





