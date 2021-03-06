
#pragma once

#define F2SM_MEM_NUMS 2
#define MEM_0_PHY_ADDR 0x31000000
#define MEM_0_SIZE 0x00800000
#define MEM_1_PHY_ADDR 0x31800000
#define MEM_1_SIZE 0x00800000

struct f2sm_mem_info
{
    void* raw_addr; 
    void* mem_addr; // mapped logical address
    int   mem_size;
};
typedef struct f2sm_mem_info f2sm_mem_info_t;


class CF2smIntf
{
public:
    CF2smIntf();
	~CF2smIntf();

    void Init();
    void testinit();
    void Close();
    void StartTransfer_up_seed(int seed, int blk_count);
    bool CheckResult(int seed, char *retbuf,char *cmpbuf,unsigned int u32WantLen);

    void ResetCounter();

    f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];
    int up_fd;
	int down_fd;
    void *virtual_base;
};
