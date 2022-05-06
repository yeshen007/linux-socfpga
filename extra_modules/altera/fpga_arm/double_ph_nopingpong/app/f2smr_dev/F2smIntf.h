#pragma once

#define F2SM_MEM_NUMS 2


struct f2sm_mem_info
{
    void* raw_addr; 
    void* mem_addr; 
    int   mem_size;
};
typedef struct f2sm_mem_info f2sm_mem_info_t;


class CF2smIntf
{
public:
    CF2smIntf();
	~CF2smIntf();
	void testinit();
    void Init();
    void Close();
    void StartTransfer_down_seed(unsigned int seed1, unsigned int seed2, int blk_count);
    bool CheckResult(u_int32_t u32WantLen);
    void ResetCounter();

    f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];
	int up_fd;
	int down_fd;
    void *virtual_base;
};
