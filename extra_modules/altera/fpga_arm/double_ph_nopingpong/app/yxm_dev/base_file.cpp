
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
static u8 *g_up_buf; 


static const char *ph0_base_in = "ph0_ref_base.dat";
static const char *ph0_change_in = "ph0_ref_1.dat";
static const char *ph0_merge_out = "ph0_merge_out.dat";
static const char *ph1_base_in = "ph1_ref_base.dat";
static const char *ph1_change_in = "ph1_ref_1.dat";
static const char *ph1_merge_out = "ph1_merge_out.dat";
//static const char *ph0_base_in = "zero.dat";
//static const char *ph0_change_in = "ph0_ref_base.dat";
//static const char *ph0_merge_out = "ph0_merge_out.dat";
//static const char *ph1_base_in = "zero.dat";
//static const char *ph1_change_in = "ph0_ref_base.dat";
//static const char *ph1_merge_out = "ph1_merge_out.dat";



static int ph0_base_in_fd;
static int ph0_change_in_fd;
static int ph0_merge_out_fd;
static int ph1_base_in_fd;
static int ph1_change_in_fd;
static int ph1_merge_out_fd;



static u32 block_size = TEST_BLOCK_SIZE;
static u32 down_blk_size = block_size;
static u32 up_blk_size = block_size/3; //1kb
static u32 up_effect_blk_size = 768;	//768

static u8 *ph0_data; 
static u8 *ph1_data;

static u8 *loop_ph0;
static u8 *loop_ph1;


#define INPUT_FILE_SIZE 6912000
#define OUTPUT_FILE_SIZE (9000 * 1024)


void base_call(void);
void change_call(void);


int main(void)
{	
	int ret;
	u32 up_i;	//收到的第几个1kb	
	u32 up_blks_one_time = 1;
	unsigned int hole_addr;

   	yxm.Init();
	g_up_fd = yxm.up_fd;
	g_down_fd = yxm.down_fd;
	loop_ph0 = (u8 *)malloc(PH1_USERBUF_OFFSET);
	loop_ph1 = (u8 *)malloc(PH1_USERBUF_OFFSET);	
	g_up_buf = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);
	
	ph0_base_in_fd = open(ph0_base_in, O_CREAT | O_RDWR); 	
	ph0_change_in_fd = open(ph0_change_in, O_CREAT | O_RDWR);
	ph0_merge_out_fd = open(ph0_merge_out, O_CREAT | O_RDWR | O_APPEND); 
	ph1_base_in_fd = open(ph1_base_in, O_CREAT | O_RDWR); 	
	ph1_change_in_fd = open(ph1_change_in, O_CREAT | O_RDWR);
	ph1_merge_out_fd = open(ph1_merge_out, O_CREAT | O_RDWR | O_APPEND); 

	yxm.InitHole();
	usleep(10000);
//	for (hole_addr = 0; hole_addr < 332; hole_addr++) {
//		yxm.WriteHole(0, hole_addr, 0xff00);
//		yxm.WriteHole(1, hole_addr, 0xcc33);
//	}
	
	yxm.Step1();
	yxm.StartTransfer_up(0, up_blks_one_time);
	yxm.Step2();
	yxm.Step3_1();	
	yxm.EnableBase();
	base_call();
	yxm.PollTest();
	yxm.CloseBase();	
	change_call();
	yxm.StartPrint();
	
	printf("start print\n");
	
#if 1
	for (up_i = 0; ; up_i++) {
		ret = read(g_up_fd, g_up_buf, up_blk_size);
		if ((u32)ret != up_blk_size) {	//打印结束中断
			if (ret == 1) {
				printf("up read normal stop\n");	
				printf("get dma %d times\n", up_i);
				write(ph0_merge_out_fd, loop_ph0, up_effect_blk_size * up_i);	//ph0
				write(ph1_merge_out_fd, loop_ph1, up_effect_blk_size * up_i);	//ph1	
				goto loop_release_free_mem;
			} else if (ret == 2) {
				fprintf(stderr, "accident stop\n");
				goto loop_release_free_mem;
			} else {
				fprintf(stderr, "up read wrong index %d\n", up_i);
				goto loop_release_free_mem;
			}
		}

		memcpy(loop_ph0 + up_i*up_effect_blk_size, g_up_buf, up_effect_blk_size);	//ph0
		memcpy(loop_ph1 + up_i*up_effect_blk_size, g_up_buf + PH1_USERBUF_OFFSET, up_effect_blk_size);	//ph1

		yxm.StartTransfer_up(0, 1);			
	}
#endif

	
#if 0
	for (up_i = 0; ; up_i++) {
		ret = read(g_up_fd, g_up_buf, up_blk_size * up_blks_one_time);
		if ((u32)ret != up_blk_size * up_blks_one_time) {	//打印结束中断
			if (ret == 1) {
				stop_print = 1;
				printf("up read normal stop\n");
				write(ph0_merge_out_fd, loop_ph0, up_effect_blk_size * up_i);	//ph0
				write(ph1_merge_out_fd, loop_ph1, up_effect_blk_size * up_i);	//ph1				
				goto loop_release_free_mem;
			} else if (ret == 2) {
				stop_print = 1;
				fprintf(stderr, "accident stop index %d\n", up_i);
				goto loop_release_free_mem;
			} else {
				stop_print = 1;
				fprintf(stderr, "up read wrong index %d\n", up_i);
				goto loop_release_free_mem;
			}
		}

		memcpy(loop_ph0 + up_i*up_blks_one_time*up_blk_size, g_up_fd, up_blk_size * up_blks_one_time);
		memcpy(loop_ph1 + up_i*up_blks_one_time*up_blk_size, g_up_fd + PH1_USERBUF_OFFSET, up_blk_size * up_blks_one_time);
		yxm.StartTransfer_up(0, up_blks_one_time);	
	}	
#endif


loop_release_free_mem:
	close(ph1_merge_out_fd);
	close(ph1_change_in_fd);
	close(ph1_base_in_fd);
	close(ph0_merge_out_fd);
	close(ph0_change_in_fd);
	close(ph0_base_in_fd);
	free(g_up_buf);
	free(loop_ph1);
	free(loop_ph0);

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

	struct f2sm_read_info_t read_info;	
	u32 down_blks_one_time = INPUT_FILE_SIZE / down_blk_size;
	
	ph0_data = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);
	ph1_data = ph0_data + PH1_USERBUF_OFFSET;	

	read(ph0_base_in_fd, ph0_data, INPUT_FILE_SIZE);
	read(ph1_base_in_fd, ph1_data, INPUT_FILE_SIZE);	

	read(g_down_fd, &read_info, sizeof(read_info));
	write(g_down_fd, ph0_data, INPUT_FILE_SIZE);
	yxm.StartTransfer_down(1, down_blks_one_time);
	
	free(ph0_data);	

}


void change_call(void)
{

	struct f2sm_read_info_t read_info;	
	u32 down_blks_one_time = INPUT_FILE_SIZE / down_blk_size;

	ph0_data = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);
	ph1_data = ph0_data + PH1_USERBUF_OFFSET;	

	read(ph0_change_in_fd, ph0_data, INPUT_FILE_SIZE);
	read(ph1_change_in_fd, ph1_data, INPUT_FILE_SIZE);	

	read(g_down_fd, &read_info, sizeof(read_info));
	write(g_down_fd, ph0_data, INPUT_FILE_SIZE);
	yxm.StartTransfer_down(1, down_blks_one_time);

	free(ph0_data);	

}





