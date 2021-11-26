
#pragma once

#define F2SM_MEM_NUMS 2

#define MEM_0_PHY_ADDR 0x30000000
#define MEM_0_SIZE 0x00800000
#define MEM_1_PHY_ADDR 0x30800000
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
    void Close();
    void StartTransfer(int mem_index,int min_l, int min_h,int shot_count);
    bool CheckResult(u_int32_t u32WantLen);

    void ResetCounter();

    f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];
    int m_fd;
    void *virtual_base;
};
