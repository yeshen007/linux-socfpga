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

static u8 linenum[16] = {15, 4, 9, 2, 12, 7, 10, 1, 14, 5, 8, 3, 13, 6, 11, 0};

static int build_another_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
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

static int check_another_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
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
				*buf_ph1++ = (line & 0xf) | (((u8)block << 4) & 0xf0);
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
	

	/* 构造和预先检查数据 */
	int ret = 0;
	int the_fd = yxm.m_fd;
	u32 block_size = TEST_BLOCK_SIZE;
	u32 block_cnt = TEST_BLOCK_CNT;	
	u8 *ph0_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	u8 *ph1_data = ph0_data + PH1_USERBUF_OFFSET;

	char *ph0_in = "./ph0-intput";
	char *ph1_in = "./ph1-intput";
	char *ph0_out = "./ph0-output";
	char *ph1_out = "./ph1-output";
	int ph0_in_fd, ph1_in_fd, ph0_out_fd, ph1_out_fd;	

	int down_i = 0;
	int down_times = block_cnt; 
	int m = 400 * 16 / 4;
	u32 down_blk_size = block_size;

	u32 up_blk_size = block_size/3;	//1kb
	u32 up_effect_blk_size = 768;	//768
	u8 *up_buf = (u8 *)malloc(up_blk_size * 2);	//2 ph each 1kb,so total 2kb
	u8 *loop_ph0 = (u8 *)malloc(PH1_USERBUF_OFFSET);
	u8 *loop_ph1 = (u8 *)malloc(PH1_USERBUF_OFFSET);
	//u8 *loop_back = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
	u32 out_loop_back_i;	//收到的第几个1kb	
	u32 done_i;

	/* 每个喷头1024个3kb有效数据，但是每个喷头buffer 12m空间 */
	ret = build_another_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Build Data Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_free_ph0_data;

	}

	ret = check_another_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Check Data Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_free_ph0_data;
	}	


	yxm.Step1();
 
	/* 启动一次上行传输 */
	yxm.StartTransfer_up(0, 1);

	yxm.Step2();

	yxm.Step3();

#if 1
	/* 
	 * step4
	 */
	 
	/* 启动下行DMA数据传输，收到中断则继续发送，直到所有数据发送完成
	 */
	ret = open(ph0_in, O_CREAT | O_RDWR | O_APPEND); 
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ph0_in_fd = ret; 
	
	ret = open(ph1_in, O_CREAT | O_RDWR | O_APPEND);
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

	/* 将数据发送给fpga */
	for (down_i = 0; down_i < m/16; down_i++) {
		ret = ioctl(the_fd, _IOC_F2SM_DQBUF, 0);
		if (ret != 0) {
			fprintf(stderr, "ioctl wrong\n");
			goto loop_release_ph1_out_fd;
		}
		
		ret = write(the_fd, ph0_data, down_blk_size * 16);
		if (ret != down_blk_size * 16) {
			fprintf(stderr, "write data to dma error\n");
			goto loop_release_ph1_out_fd;
		}
		yxm.StartTransfer_down(1, 16);		
		
	}
	
	/* 将总共发送的数据写入数据输入文件 */
	for (down_i = 0; down_i < m/16; down_i++) {
		ret = write(ph0_in_fd, ph0_data, down_blk_size * 16);
		if (ret != down_blk_size * 16) {
			fprintf(stderr, "write data to ph0 input file error\n");
			goto loop_release_ph1_out_fd;
		}

		ret = write(ph1_in_fd, ph1_data, down_blk_size * 16);
		if (ret != down_blk_size * 16) {
			fprintf(stderr, "write data to ph1 input file error\n");
			goto loop_release_ph1_out_fd;
		}
		
	}
	
	yxm.StartPrint();
	printf("start print\n");

	/* 打印过程持续检测上行DMA中断。每次收到中断则启动下次一数据传输(配置数据块大小,地址6写1写0)
	 * 此过程软件不需要主动结束，有中断上来就一直配置传输
	 * 打印过程中持续检测硬件中断bit[5]和bit[4]。收到硬件中断bit[4]表示打印正常结束/收到硬件
	 * 中断bit[5]表示打印异常停止。打印输出寄存器内容,测试结束
	 */	
	for (out_loop_back_i = 0; ; out_loop_back_i++) {
		ret = read(the_fd, up_buf, up_blk_size);			//驱动中实现每个喷头1kb
		//ret = read(the_fd, loop_back + out_loop_back_i*2*up_blk_size, up_blk_size);
		if (ret != up_blk_size) {
			if (ret == 1) {
				printf("normal stop\n");
				printf("get interrupt times:%d\n", out_loop_back_i);
				printf("each ph get data %d bytes\n", out_loop_back_i*up_blk_size);
#if 0				
				/* 将总共收到的数据写入数据输出文件 */
				for (done_i = 0; done_i < out_loop_back_i; done_i++) {
					ret = write(ph0_out_fd, loop_back + done_i*2*up_blk_size, up_blk_size);
					if (ret != up_blk_size) {
						fprintf(stderr, "write data to ph0 output file error\n");
						goto loop_skit;
					}					
					write(ph0_out_fd, loop_back + done_i*2*up_blk_size + up_blk_size, up_blk_size);
					if (ret != up_blk_size) {
						fprintf(stderr, "write data to ph1 output file error\n");
						goto loop_skit;
					}					
				}
#endif			
				/* 将总共收到的数据写入数据输出文件 */
				ret = write(ph0_out_fd, loop_ph0, up_effect_blk_size * out_loop_back_i);	//ph0
				if (ret != up_effect_blk_size * out_loop_back_i) {
					fprintf(stderr, "write data to ph0 output file error\n");
					goto loop_skit;
				}
				ret = write(ph1_out_fd, loop_ph1, up_effect_blk_size * out_loop_back_i);	//ph1
				if (ret != up_effect_blk_size * out_loop_back_i) {
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
				fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);	
				goto loop_skit;
			}
			
		} 
		
		/* 将每次收到的数据追加写入数据输出文件buf */
		//memcpy(loop_ph0 + out_loop_back_i*up_blk_size, up_buf, up_blk_size);	//ph0
		//memcpy(loop_ph1 + out_loop_back_i*up_blk_size, up_buf + up_blk_size, up_blk_size);	//ph1
		memcpy(loop_ph0 + out_loop_back_i*up_effect_blk_size, up_buf, up_effect_blk_size);	//ph0
		memcpy(loop_ph1 + out_loop_back_i*up_effect_blk_size, up_buf + up_blk_size, up_effect_blk_size);	//ph1

		//printf("success get one shanging interrupt\n");
		yxm.StartTransfer_up(0, 1);
			
	}

loop_skit:
	//free(loop_back);	
	free(loop_ph1);
	free(loop_ph0);
	free(up_buf);		

#endif

#if 0
	/* step4 */
	pid_t pid;
	pid = fork();
	if (pid == 0) {		/* 下行子进程 ioctl write StartTransfer_down close */
		/* 下行DMA数据传输，收到中断则继续发送，直到所有数据发送完成 */
		int down_i = 0;
		int down_times = block_cnt;	
		u32 down_blk_size = block_size;

		/* 第一次ioctl */
		ret = ioctl(the_fd, _IOC_F2SM_DQBUF, 0);
		if (ret != 0) {
			fprintf(stderr, "first ioctl wrong\n");
			exit(-1);
		}
		ret = write(the_fd, ph0_data + down_i * down_blk_size, down_blk_size);
		if (ret != down_blk_size) {
			fprintf(stderr, "first write error\n");
			exit(-1);	
		}
		yxm.StartTransfer_down(1, 1);	

		yxm.StartPrint();


		for (down_i = 1; down_i < down_times; down_i++) {
	
			ret = ioctl(the_fd, _IOC_F2SM_DQBUF, 0);
			if (ret < 0) {
				fprintf(stderr, "Ioctl Error at line %d, file %s\n", __LINE__, __FILE__);
				yxm.Close();
				exit(-1);
			} else if (ret == 1) {
				printf("normal stop\n");
				yxm.Close();
				exit(0);
			} else if (ret == 2) {
				fprintf(stderr, "accident stop\n");
				yxm.Close();
				exit(-1);
			}

			ret = write(the_fd, ph0_data + down_i * down_blk_size, down_blk_size);
			if (ret != down_blk_size) {
				printf("write error\n");
				printf("write done %d times\n", down_i);
				return -1;		
			}

			yxm.StartTransfer_down(1, 1);
		
		}	
		printf("data send done\n");
		yxm.Close();
		exit(0);
		
	} else if (pid > 0) {	/* 上行主进程 read StartTransfer_up waitpid */
		
		yxm.StartPrint();
		
		/* 打印过程持续检测上行DMA中断。每次收到中断则启动下次一数据传输(配置数据块大小,地址6写1写0)
		 * 此过程软件不需要主动结束，有中断上来就一直配置传输
		 * 打印过程中持续检测硬件中断bit[5]和bit[4]。收到硬件中断bit[4]表示打印正常结束/收到硬件
		 * 中断bit[5]表示打印异常停止。打印输出寄存器内容,测试结束
		 */
		u32 up_blk_size = block_size/3;	//1kb
		u8 *up_buf = (u8 *)malloc(up_blksize * 2);	//2 ph each 1kb,so total 2kb
		u8 *loop_back_data = (u8 *)malloc(2 * PH1_USERBUF_OFFSET);
		u32 out_loop_back_i;	//收到的第几个1kb

		for (out_loop_back_i = 0; ; out_loop_back_i++) {
			ret = read(the_fd, up_buf, up_blksize * 2);
			if (ret != up_blksize) {
				if (ret == 1) {
					printf("normal stop\n");
					free(up_buf);
					goto loop_skit;
				} else if (ret == 2) {
					fprintf(stderr, "accident stop\n");
					free(up_buf);
					goto loop_skit;
				} else {
					fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
					free(up_buf);
					free(loop_back_data);					
					yxm.Close();
					exit(-1);
				}
				
			} 
			
			yxm.StartTransfer_up(0, 1);

		}

		
		/* TODO:拉偏置和效验 */
		
loop_skit:
	    /* 打印寄存器
	     * 结束打印
		 */
		free(loop_back_data);
		yxm.PrintRegs();
		yxm.FinishPrint();			
		yxm.Close();	
		waitpid(pid, NULL, 0);	
		
	} else {
		fprintf(stderr, "Fork Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
#endif	

loop_release_ph1_out_fd:
	close(ph1_out_fd);

loop_release_ph0_out_fd:
	close(ph0_out_fd);

loop_release_ph1_in_fd:
	close(ph1_in_fd);

loop_release_ph0_in_fd:
	close(ph0_in_fd);

loop_free_ph0_data:
	free(ph0_data);

	yxm.PrintRegs();
	yxm.FinishPrint();
	yxm.Close();	

	return ret;
}



