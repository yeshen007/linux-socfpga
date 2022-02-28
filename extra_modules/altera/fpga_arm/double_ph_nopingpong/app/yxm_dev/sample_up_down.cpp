
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
#include "list.h"

#define NODE_EMPTY 0
#define NODE_FULL 4
#define INIT_MEM_INDEX -1	//mean no mem

typedef struct f2sm_list_head
{
	int count;
	int current;
	struct list_head list;
}f2sm_list_head_t;

typedef struct f2sm_list_node
{
	int mem_index;
	struct list_head list;
}f2sm_list_node_t;

static inline void init_f2sm_list(f2sm_list_head_t *head)
{
	head->count = NODE_EMPTY;
	head->current = INIT_MEM_INDEX;
	INIT_LIST_HEAD(&head->list);
} 

static inline int is_f2sm_list_full(f2sm_list_head_t *head)
{
	return READ_ONCE(head->count) == NODE_FULL;
} 

static inline int is_f2sm_list_empty(f2sm_list_head_t *head)
{
	return READ_ONCE(head->count) == NODE_EMPTY;
} 

/* 使用前先用is_f2sm_list_full检查 */
static inline void f2sm_enque(f2sm_list_node_t *node, f2sm_list_head_t *head)
{
	list_add_tail(&node->list, &head->list);
	head->count++;
	head->current = (head->current + 1) & 0x3;
} 

/* 使用前先用is_f2sm_list_empty检查 */
static inline f2sm_list_node_t *f2sm_deque(f2sm_list_head_t *head)
{
	f2sm_list_node_t *node = list_entry(head->list.next, f2sm_list_node_t, list);
	list_del(&node->list);
	head->count--; 
	if (is_f2sm_list_empty(head))
		head->current = INIT_MEM_INDEX;
	return node;
} 

static f2sm_list_head_t g_f2sm_list_head;

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define TEST_TIMES 50

#define HPS_FPGA_BRIDGE_BASE 0xFF200000


#define TEST_BLOCK_SIZE (3*1024)
#define TEST_BLOCK_CNT 1024

#define PH1_USERBUF_OFFSET 	(16*1024*1024)

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
static const char *ph0_in_name = "ph0_ref.dat";
static const char *ph1_in_name = "ph1_ref.dat";

static int ph0_in_fd;
static int ph1_in_fd;


static u32 block_size = TEST_BLOCK_SIZE;	//3kb
static u32 down_blk_size = block_size;	//3kb
static u32 up_blk_size = block_size/3; //1kb
//static u32 up_effect_blk_size = 768;	//768

static u8 *ph0_data; 
static u8 *ph1_data;

static u8 *up_buf; 

static u8 *mem[NODE_FULL];

static int start_print = 0;
static int stop_print = 0;

#define INPUT_FILE_SIZE 6912000
#define OUTPUT_FILE_SIZE (9000 * 1024)

static u32 down_blks_one_time = INPUT_FILE_SIZE / down_blk_size;
static u32 up_blks_one_time = OUTPUT_FILE_SIZE / up_blk_size;	//9000

static struct stat ph0_in_stat;
static struct stat ph1_in_stat;

void *up_thread(void *arg);
void *down_thread(void *arg);


int main(void)
{	

	up_blks_one_time = up_blks_one_time / 100;

	int ret = 0;
	u32 up_i;	//收到的第几个dma
	u32 _up_i;	
	pthread_t up_tid;
	pthread_t down_tid;
	void *up_ret;
	void *down_ret;
	f2sm_list_node_t *p_node;
	int node_index;
	cpu_set_t set;
	unsigned int hole_addr;
	unsigned short hole_val;
	
	init_f2sm_list(&g_f2sm_list_head);

   	yxm.Init();

	yxm.InitHole();
	usleep(10000);
	for (hole_addr = 0; hole_addr < 332; hole_addr++) {
		yxm.WriteHole(0, hole_addr, 0xff00);
		yxm.WriteHole(1, hole_addr, 0xcc33);
	}

//	for (hole_addr = 0; hole_addr < 332; hole_addr++) {
//		yxm.ReadHole(0, hole_addr, &hole_val);
//		printf("init ph 0 addr %u val 0x%x\n", hole_addr, hole_val);
//	}	
	
	
	up_fd = yxm.up_fd;
	down_fd = yxm.down_fd;	

	up_buf = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);	
	mem[0] = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);
	mem[1] = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);	
	mem[2] = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);
	mem[3] = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);


	ret = open(ph0_in_name, O_CREAT | O_RDWR); 
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}
	ph0_in_fd = ret; 
	fstat(ph0_in_fd, &ph0_in_stat);
	
	ret = open(ph1_in_name, O_CREAT | O_RDWR);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph0_in_fd;
	}	
	ph1_in_fd = ret; 
	fstat(ph1_in_fd, &ph1_in_stat);
	
	if ((ph0_in_stat.st_size != ph1_in_stat.st_size) ||
		(ph0_in_stat.st_size != 6912000)) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph0_in_fd;
	}

	yxm.Step1();
 
	/* 启动一次上行传输 */
	yxm.StartTransfer_up(0, up_blks_one_time);

	yxm.Step2();

	yxm.Step3();
	

	/* 查看和设置主线程亲和性 */
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	sched_setaffinity(0, sizeof(set), &set);
	
	
	CPU_ZERO(&set);
	sched_getaffinity(0, sizeof(set), &set);
	if (CPU_ISSET(0, &set)) 
		printf("main thread cpu 0 is set\n");

	if (CPU_ISSET(1, &set)) 
		printf("main thread cpu 1 is set\n");


	/* 上行写文件子线程 */
	ret = pthread_create(&up_tid, NULL, up_thread, NULL);
	if (ret != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_ph1_in_fd;
	}	

	/* 下行发送数据子线程 */
	ret = pthread_create(&down_tid, NULL, down_thread, NULL); 
	if (ret != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		goto loop_release_up_thread;
	}

	while (start_print != 1)
		usleep(5000);

	/* 开始打印 */	
	yxm.StartPrint();
	printf("start print\n");


	for (up_i = 0; ; up_i++) {
		ret = read(up_fd, up_buf, up_blk_size * up_blks_one_time);
		if ((u32)ret != up_blk_size * up_blks_one_time) {	//打印结束中断
			if (ret == 1) {
				stop_print = 1;
				printf("up read normal stop\n");	
				goto loop_release_down_thread;
			} else if (ret == 2) {
				stop_print = 1;
				fprintf(stderr, "accident stop index %d\n", up_i);
				goto loop_release_down_thread;
			} else {
				stop_print = 1;
				fprintf(stderr, "up read wrong index %d\n", up_i);
				goto loop_release_down_thread;
			}
		}
#if 1
		/* 抽样起始dma序号被500整除的图 */
		if ((up_i == 0) || (up_i % 500 != 0)) {
			yxm.StartTransfer_up(0, up_blks_one_time);
			continue;
		}
		
		/* 先看队列还有没有空间 */
		if (is_f2sm_list_full(&g_f2sm_list_head)) {
			yxm.StartTransfer_up(0, up_blks_one_time);
			continue;
		}

		/* 将抽样到的dma连着接下来的连续99个dma共100个写到一块连续的用户内存再放入队列 */
		node_index = (g_f2sm_list_head.current + 1) & 0x3;  	// 获取这块存放100个dma的连续内存的mem index
		memcpy(mem[node_index], up_buf, up_blk_size * up_blks_one_time);
		memcpy(mem[node_index] + PH1_USERBUF_OFFSET, up_buf + PH1_USERBUF_OFFSET, up_blk_size * up_blks_one_time);
		yxm.StartTransfer_up(0, up_blks_one_time);
		
		for (_up_i = 1; _up_i < 100; _up_i++) {
			ret = read(up_fd, up_buf, up_blk_size * up_blks_one_time);
			if ((u32)ret != up_blk_size * up_blks_one_time) {
				if (ret == 1) {
					stop_print = 1;
					printf("sample read normal stop\n");	
					goto loop_release_down_thread;					
				}
				stop_print = 1;
				fprintf(stderr, "sample read wrong up_i %d _up_i %d\n", up_i, _up_i);
				goto loop_release_down_thread;				
			}
			memcpy(mem[node_index] + up_blk_size * up_blks_one_time * _up_i, 
					up_buf, up_blk_size * up_blks_one_time);
			memcpy(mem[node_index] + PH1_USERBUF_OFFSET + up_blk_size * up_blks_one_time * _up_i, 
					up_buf + PH1_USERBUF_OFFSET, up_blk_size * up_blks_one_time);
			yxm.StartTransfer_up(0, up_blks_one_time);
		}

		up_i = up_i + _up_i - 1;	//因为外面还会up_i++
		 
		p_node = (f2sm_list_node_t *)malloc(sizeof(f2sm_list_node_t));
		p_node->mem_index = node_index;

		f2sm_enque(p_node, &g_f2sm_list_head);
	
#endif
		//yxm.StartTransfer_up(0, up_blks_one_time);
	}

	
loop_release_down_thread:
	ret = pthread_join(down_tid, &down_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (down_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	
loop_release_up_thread:
	ret = pthread_join(up_tid, &up_ret);
	if (ret != 0) 
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
	if (up_ret == NULL)
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);	

loop_release_ph1_in_fd:
	close(ph1_in_fd);

loop_release_ph0_in_fd:
	close(ph0_in_fd);

	free(mem[3]);
	free(mem[2]);
	free(mem[1]);
	free(mem[0]);

	free(up_buf);
	
	yxm.PrintRegs();
	yxm.FinishPrint();
	yxm.Close();	

	return ret;
}

void *up_thread(void *arg)
{
	u32 up_i;
	int tmp_fd;
	int mem_index;
	char tmp_name[20] = {0};
	f2sm_list_node_t *p_node;
	cpu_set_t set;

	/* 查看和up线程亲和性 */
	CPU_ZERO(&set);
	CPU_SET(1, &set);
	sched_setaffinity(0, sizeof(set), &set);
	
	CPU_ZERO(&set);
	sched_getaffinity(0, sizeof(set), &set);
	if (CPU_ISSET(0, &set)) 
		printf("up thread cpu 0 is set\n");

	if (CPU_ISSET(1, &set)) 
		printf("up thread cpu 1 is set\n");	
	

	for (up_i = 0; stop_print != 1; ) {

		if (is_f2sm_list_empty(&g_f2sm_list_head)) {
			usleep(5000);
			continue;
		}

		//printf("get one mem\n");
		
		p_node = f2sm_deque(&g_f2sm_list_head);
		mem_index = p_node->mem_index;
		
		memset(tmp_name, 0, sizeof(tmp_name));
		sprintf(tmp_name, "ph0_out_%d.dat", up_i);
		tmp_fd = open(tmp_name, O_CREAT | O_RDWR | O_APPEND); 
		write(tmp_fd, mem[mem_index], OUTPUT_FILE_SIZE);
		close(tmp_fd);
		
		memset(tmp_name, 0, sizeof(tmp_name));
		sprintf(tmp_name, "ph1_out_%d.dat", up_i);
		tmp_fd = open(tmp_name, O_CREAT | O_RDWR | O_APPEND); 
		write(tmp_fd, mem[mem_index] + PH1_USERBUF_OFFSET, OUTPUT_FILE_SIZE);
		close(tmp_fd);

		free(p_node);
		
		up_i++;
	}
	
	return (void *)1;
}



void *down_thread(void *arg)
{
	int ret;
	int down_i;
	struct f2sm_read_info_t read_info;	

	ph0_data = (u8 *)malloc(PH1_USERBUF_OFFSET * 2);
	ph1_data = ph0_data + PH1_USERBUF_OFFSET;	

	read(ph0_in_fd, ph0_data, INPUT_FILE_SIZE);
	read(ph1_in_fd, ph1_data, INPUT_FILE_SIZE);	
	
	//先发送一块dma
	ret = read(down_fd, &read_info, sizeof(read_info));
	if ((u32)ret != sizeof(read_info) || read_info.mem_index != 0) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}

	ret = write(down_fd, ph0_data, INPUT_FILE_SIZE);
	if ((u32)ret != 6912000) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		return NULL;
	}
	yxm.StartTransfer_down(1, down_blks_one_time);

	start_print = 1;

	//一直发送到测试次数结束
	for (down_i = 1; down_i < TEST_TIMES; down_i++) {
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
		/* 将ph0_data中的down_blk_size发送给fpga */
		ret = write(down_fd, ph0_data, INPUT_FILE_SIZE);
		if ((u32)ret != 6912000) {
			fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
			return NULL;
		}
		yxm.StartTransfer_down(1, down_blks_one_time);

	}

	free(ph0_data);	
	
	return (void *)1;
}



