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


#define TEST_BLOCK_SIZE (3*1024)
#define TEST_BLOCK_CNT 1024

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
static const char *ph0_in = "ph0_ref.dat";
static const char *ph1_in = "ph1_ref.dat";
static const char *ph0_out = "ph0_out.dat";
static const char *ph1_out = "ph1_out.dat";
static int ph0_in_fd;
static int ph1_in_fd;
static int ph0_out_fd;
static int ph1_out_fd;

static u32 block_size = TEST_BLOCK_SIZE;
static u32 down_blk_size = block_size;
static u32 up_blk_size = block_size/3; //1kb
static u32 up_effect_blk_size = 768;	//768

static u8 *ph0_data; 
static u8 *ph1_data;

static u8 *up_buf; 
static u8 *loop_ph0;
static u8 *loop_ph1;

static int start_print = 0;


void *down_thread(void *arg);


int main(void)
{	
	int ret = 0;
	u32 up_i;	//收到的第几个1kb	
	pthread_t down_tid;
	void *down_ret;

   	yxm.Init();
	up_fd = yxm.up_fd;
	down_fd = yxm.down_fd;	


	ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	ph1_data = ph0_data + PH1_USERBUF_OFFSET;

	up_buf = (u8 *)malloc(up_blk_size * 2);			//2 ph each 1kb,so total 2kb
	loop_ph0 = (u8 *)malloc(PH1_USERBUF_OFFSET);
	loop_ph1 = (u8 *)malloc(PH1_USERBUF_OFFSET);


	yxm.Step1();
 
	/* 启动一次上行传输 */
	yxm.StartTransfer_up(0, 1);

	yxm.Step2();

	yxm.Step3();


	/* 
	 * step4
	 */
	ret = open(ph0_in, O_CREAT | O_RDWR); 
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ph0_in_fd = ret; 
	
	ret = open(ph1_in, O_CREAT | O_RDWR);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph0_in_fd;
	}	
	ph1_in_fd = ret; 

	ret = open(ph0_out, O_CREAT | O_RDWR | O_APPEND); 
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph1_in_fd;
	}
	ph0_out_fd = ret; 

	ret = open(ph1_out, O_CREAT | O_RDWR | O_APPEND);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph0_out_fd;
	}
	ph1_out_fd = ret; 


	/* 下行子线程 */
	ret = pthread_create(&down_tid, NULL, down_thread, NULL); 
	if (ret != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph1_out_fd;
	}


	while (start_print != 1)
		usleep(5000);

	/* 开始打印 */	
	yxm.StartPrint();
	printf("start print\n");


	/* 打印过程持续检测上行DMA中断。每次收到中断则启动下次一数据传输(配置数据块大小,地址6写1写0)
	 * 此过程软件不需要主动结束，有中断上来就一直配置传输
	 * 打印过程中持续检测硬件中断bit[5]和bit[4]。收到硬件中断bit[4]表示打印正常结束/收到硬件
	 * 中断bit[5]表示打印异常停止。打印输出寄存器内容,测试结束
	 */	
	for (up_i = 0; ; up_i++) {
		ret = read(up_fd, up_buf, up_blk_size);
		if ((u32)ret != up_blk_size) {	//打印结束中断
			if (ret == 1) {
				printf("up read normal stop\n");
				printf("up read get interrupt times:%d\n", up_i);
				printf("up read each ph get data %d bytes\n", up_i*up_blk_size);
#if 1				
				ret = write(ph0_out_fd, loop_ph0, up_effect_blk_size * up_i);	//ph0
				if ((u32)ret != up_effect_blk_size * up_i) {
					fprintf(stderr, "write data to ph0 output file error\n");
					goto loop_release_down_thread;
				}
				ret = write(ph1_out_fd, loop_ph1, up_effect_blk_size * up_i);	//ph1
				if ((u32)ret != up_effect_blk_size * up_i) {
					fprintf(stderr, "write data to ph1 output file error\n");
					goto loop_release_down_thread;
				}
#endif				
				goto loop_release_down_thread;
			} else if (ret == 2) {
				fprintf(stderr, "accident stop\n");
				goto loop_release_down_thread;
			} else {
				fprintf(stderr, "up read wrong index %d\n", up_i);
				goto loop_release_down_thread;
			}
		}
#if 0
		/* 将该次dma中断收到的数据直接写入文件 */
		ret = write(ph0_out_fd, up_buf, up_effect_blk_size);
		if ((u32)ret != up_effect_blk_size) {
			fprintf(stderr, "write data to ph0 output file error\n");
			goto loop_release_down_thread;
		}
		ret = write(ph1_out_fd, up_buf + up_blk_size, up_effect_blk_size);
		if ((u32)ret != up_effect_blk_size) {
			fprintf(stderr, "write data to ph1 output file error\n");
			goto loop_release_down_thread;
		}	
#endif
#if 1
		memcpy(loop_ph0 + up_i*up_effect_blk_size, up_buf, up_effect_blk_size);	//ph0
		memcpy(loop_ph1 + up_i*up_effect_blk_size, up_buf + up_blk_size, up_effect_blk_size);	//ph1
#endif
		yxm.StartTransfer_up(0, 1);
	}

	
loop_release_down_thread:
	ret = pthread_join(down_tid, &down_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (down_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);

loop_release_ph1_out_fd:
	close(ph1_out_fd);

loop_release_ph0_out_fd:
	close(ph0_out_fd);

loop_release_ph1_in_fd:
	close(ph1_in_fd);

loop_release_ph0_in_fd:
	close(ph0_in_fd);

	free(loop_ph1);
	free(loop_ph0);
	free(up_buf);
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

	//先发送一块dma
	ret = read(down_fd, &read_info, sizeof(read_info));
	if ((u32)ret != sizeof(read_info) || read_info.mem_index != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	read(ph0_in_fd, ph0_data, down_blk_size);
	read(ph1_in_fd, ph1_data, down_blk_size);
	ret = write(down_fd, ph0_data, down_blk_size);
	if ((u32)ret != down_blk_size) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	yxm.StartTransfer_down(1, 1);

	start_print = 1;

	//发送第二块到最后一块
	for (down_i = 1; ; down_i++) {
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
		/* 从文件ph0_in_fd读down_blk_size到ph0_data */
		read(ph0_in_fd, ph0_data, down_blk_size);
		ret = read(ph1_in_fd, ph1_data, down_blk_size);
		if (ret == 0)
			break;	
		/* 将ph0_data中的down_blk_size发送给fpga */
		ret = write(down_fd, ph0_data, down_blk_size);
		if ((u32)ret != down_blk_size) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}
		yxm.StartTransfer_down(1, 1);
	}

	
	return (void *)1;
}


