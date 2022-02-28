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



int main(void)
{
	struct f2sm_read_info_t read_info;
	
	CYxmIntf yxm;

   	yxm.Init();
	

	/* 构造和预先检查数据 */
	int ret = 0;
	int up_fd = yxm.up_fd;
	int down_fd = yxm.down_fd;
	u32 block_size = TEST_BLOCK_SIZE;
	u8 *ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	u8 *ph1_data = ph0_data + PH1_USERBUF_OFFSET;

	const char *ph0_in = "ph0_ref.dat";
	const char *ph1_in = "ph1_ref.dat";
	const char *ph0_out = "ph0_out.dat";
	const char *ph1_out = "ph1_out.dat";
	
	int ph0_in_fd, ph1_in_fd, ph0_out_fd, ph1_out_fd;	


	u32 down_blk_size = block_size;

	u32 up_blk_size = block_size/3;	//1kb
	u32 up_effect_blk_size = 768;	//768
	u8 *up_buf = (u8 *)malloc(up_blk_size * 2);	//2 ph each 1kb,so total 2kb
	u8 *loop_ph0 = (u8 *)malloc(PH1_USERBUF_OFFSET);
	u8 *loop_ph1 = (u8 *)malloc(PH1_USERBUF_OFFSET);

	u32 out_loop_back_i;	//收到的第几个1kb	

	int down_i;


	yxm.Step1();
 
	/* 启动一次上行传输 */
	yxm.StartTransfer_up(0, 1);

	yxm.Step2();

	yxm.Step3();

	/* 
	 * step4
	 */
	 
	/* 启动下行DMA数据传输，收到中断则继续发送，直到所有数据发送完成
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

#if 0
	/* 先一次性将输入文件的数据全部读到buf中 */
	ret = read(ph0_in_fd, ph0_data, PH1_USERBUF_OFFSET);
	printf("ph0_ref.dat size %d\n", ret);

	ret = read(ph1_in_fd, ph1_data, PH1_USERBUF_OFFSET);
	printf("ph1_ref.dat size %d\n", ret);


	/* 将数据发送给fpga */
	down_i_end = ret / 3072;
	for (down_i = 0; down_i < down_i_end; down_i++) {
		ret = ioctl(the_fd, _IOC_F2SM_DQBUF, 0);
		if (ret != 0) {
			fprintf(stderr, "ioctl wrong donw index %d\n", down_i);
			goto loop_release_ph1_out_fd;
		}	

		/* 将3kb ph0_ref.dat和3kb ph1_ref.dat的数据写到dma内存 */
		ret = write(the_fd, ph0_data + down_i*down_blk_size, down_blk_size);
		if (ret != down_blk_size) {
			fprintf(stderr, "write data to dma error\n");
			goto loop_release_ph1_out_fd;
		}
		yxm.StartTransfer_down(1, 1);		
	}
#endif

#if 0
	/* 一个循环从输入文件读取3kb发送给dma内存然后启动传输 */
	for (down_i = 0; ; down_i++) {
		ret = ioctl(the_fd, _IOC_F2SM_DQBUF, 0);
		if (ret != 0) {
			fprintf(stderr, "ioctl wrong donw index %d\n", down_i);
			goto loop_release_ph1_out_fd;
		}
		
		read(ph0_in_fd, ph0_data, down_blk_size);
		ret = read(ph1_in_fd, ph1_data, down_blk_size);
		if (ret == 0)
			break;	
		
		ret = write(the_fd, ph0_data, down_blk_size);
		if ((u32)ret != down_blk_size) {
			fprintf(stderr, "write data to dma error\n");
			goto loop_release_ph1_out_fd;
		}
		yxm.StartTransfer_down(1, 1);
	}
#endif

#if 1
	/* 一个循环从输入文件读取3kb发送给dma内存然后启动传输 */
	for (down_i = 0; ; down_i++) {
		
		ret = read(down_fd, &read_info, sizeof(read_info));
		if ((u32)ret != sizeof(read_info)) {
			if (ret == 1) {
				printf("down read normal stop\n");
				goto loop_skit;
			} else if (ret ==2) {
				printf("down read accident stop\n");
				goto loop_skit;
			} else {
				fprintf(stderr, "down read wrong index %d\n", down_i);
				goto loop_release_ph1_out_fd;
			}
		}	
		if (read_info.mem_index != 0) {
			ret = -1;
			fprintf(stderr, "shit meeting ghost...\n");
			goto loop_release_ph1_out_fd;
		}
		
		read(ph0_in_fd, ph0_data, down_blk_size);
		ret = read(ph1_in_fd, ph1_data, down_blk_size);
		if (ret == 0)
			break;	
		ret = write(down_fd, ph0_data, down_blk_size);
		if ((u32)ret != down_blk_size) {
			fprintf(stderr, "down write wrong index %d\n", down_i);
			goto loop_release_ph1_out_fd;
		}
		
		yxm.StartTransfer_down(1, 1);
	}
#endif


	/* 开始打印 */	
	yxm.StartPrint();
	printf("start print\n");

	/* 打印过程持续检测上行DMA中断。每次收到中断则启动下次一数据传输(配置数据块大小,地址6写1写0)
	 * 此过程软件不需要主动结束，有中断上来就一直配置传输
	 * 打印过程中持续检测硬件中断bit[5]和bit[4]。收到硬件中断bit[4]表示打印正常结束/收到硬件
	 * 中断bit[5]表示打印异常停止。打印输出寄存器内容,测试结束
	 */	
	for (out_loop_back_i = 0; ; out_loop_back_i++) {
		ret = read(up_fd, up_buf, up_blk_size);			//驱动中实现每个喷头1kb
		if ((u32)ret != up_blk_size) {
			if (ret == 1) {
				printf("up read normal stop\n");
				printf("up read get interrupt times:%d\n", out_loop_back_i);
				printf("up read each ph get data %d bytes\n", out_loop_back_i*up_blk_size);
			
				/* 将总共收到的数据写入数据输出文件 */
				ret = write(ph0_out_fd, loop_ph0, up_effect_blk_size * out_loop_back_i);	//ph0
				if ((u32)ret != up_effect_blk_size * out_loop_back_i) {
					fprintf(stderr, "write data to ph0 output file error\n");
					goto loop_skit;
				}
				ret = write(ph1_out_fd, loop_ph1, up_effect_blk_size * out_loop_back_i);	//ph1
				if ((u32)ret != up_effect_blk_size * out_loop_back_i) {
					fprintf(stderr, "write data to ph1 output file error\n");
					goto loop_skit;
				}	
				printf("each ph write effect data %d\n", ret);
				ret = 1;
				goto loop_skit;
			} else if (ret == 2) {
				fprintf(stderr, "accident stop\n");
				goto loop_skit;
			} else {
				fprintf(stderr, "up read wrong index %d\n", out_loop_back_i);
				goto loop_skit;
			}
			
		} 
		
		/* 将每次收到的数据追加写入数据输出文件buf */
		memcpy(loop_ph0 + out_loop_back_i*up_effect_blk_size, up_buf, up_effect_blk_size);	//ph0
		memcpy(loop_ph1 + out_loop_back_i*up_effect_blk_size, up_buf + up_blk_size, up_effect_blk_size);	//ph1

		//printf("success get one shanging interrupt\n");
		yxm.StartTransfer_up(0, 1);
			
	}
	

loop_skit:	

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



