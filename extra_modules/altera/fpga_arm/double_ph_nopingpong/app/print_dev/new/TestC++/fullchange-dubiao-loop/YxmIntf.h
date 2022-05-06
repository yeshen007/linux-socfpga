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


class CYxmIntf
{
public:
    int up_fd;
	int down_fd;
    void *virtual_base;
	f2sm_mem_info_t mem_info[F2SM_MEM_NUMS];

    CYxmIntf();
	~CYxmIntf();
	
    void Init();
    void Close();
	void StartTransfer_up(int up_mem_index, unsigned int blk_count); 
	void StartTransfer_down(int down_mem_index, unsigned int blk_count);

	void ConfigMaster();
	void ConfigSlave();		
	void PrintEn();
	void PrintDi();
	void SyncSimEn();
	void TestDataEn();
	void LoopTestEn();
	void BaseImageEn();
	void BaseImageDi();

	void ConfigRasterSim();
	void ConfigPdSim();
	void ConfigDivosr();
	void ConfigJob();
	void ConfigOffset();
	void ConfigFire 	();
	void ConfigSwitch();
	void ConfigLabel();
	
	void PrintRegs();
};

