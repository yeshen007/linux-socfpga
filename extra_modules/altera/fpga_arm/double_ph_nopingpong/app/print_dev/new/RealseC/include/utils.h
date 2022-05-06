#ifndef _UTILS_H_
#define _UTILS_H_


typedef unsigned char u8;
typedef unsigned int u32;

#define TEST_BLOCK_SIZE (3*1024)
#define TEST_BLOCK_CNT 1024
#define PH2_USERBUF_OFFSET 	(16*1024*1024)

struct f2sm_read_info_t {
	u32 irq_count;
	int mem_index;
};


int build_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt);
int check_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt);


#endif
