#pragma once

#define F2SM_MEM_NUMS 2

#define MEM_PHY_UP 0x30000000
#define MEM_PHY_UP_SIZE 0x02000000
#define MEM_PHY_DOWN 0x32000000
#define MEM_PHY_DOWN_SIZE 0x02000000
#define PH1_OFFSET 0x01000000



struct f2sm_mem_info
{
    void* raw_addr; 
    void* mem_addr; // mapped logical address
    int   mem_size;
};
typedef struct f2sm_mem_info f2sm_mem_info_t;


class CYxmIntf
{
public:
    CYxmIntf();
	~CYxmIntf();
    void Init();
    void Close();
	void StartTransfer_up(int mem_index, unsigned int blk_count);
	void StartTransfer_down(int mem_index, unsigned int blk_count);
	void StartPrint();
	void FinishPrint();
	void Step1();
	void Step2();
	void Step3();
	void Step3_1();
	void PrintRegs();

    int up_fd;
	int down_fd;
    void *virtual_base;
	f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];
};

