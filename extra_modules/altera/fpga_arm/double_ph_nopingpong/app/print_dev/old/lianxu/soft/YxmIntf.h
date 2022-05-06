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

/* fpga regs about print */
#define PRINT_OPEN 37
#define MASTER_SYNC_SIM_EN 52
#define O_MASTER_DPI_D_FACTOR 53
#define O_MASTER_DPI_M_FACTOR 54
#define O_MASTER_DPI_T_FACTOR 55
#define O_MASTER_RASTER_SEL	60
#define	O_MASTER_RASTER_SIM_PERIOD 62
#define O_MASTER_PD_SEL 70
#define	O_MASTER_PD_SIM_NUMBER 73
#define	O_MASTER_PD_SIM_LENGTH 74
#define	O_MASTER_PD_SIM_INTERVAL 75
#define	O_PD_CHK_THRESHOLD 80
#define	O_PD_SERACH_MODE 90
#define	O_PD_SEARCH_INTERVAL 91
#define	O_PD_SEARCH_WINDOW 92
#define	O_PD_VIRTUAL_EN 93
#define O_DDR_BUFF_DEEP 100
#define O_WORK_MODE	110
#define O_JOB_FIRE_NUM 113
#define O_BASE_IMAGE_EN 114
#define	O_TEST_DATA_EN 140
#define	O_LOOP_TEST_EN 145



class CYxmIntf
{
public:
    CYxmIntf();
	~CYxmIntf();
    void Init();
    void Close();
    //void StartTransfer(int mem_index, unsigned int seed1, unsigned int seed2, unsigned int blk_count);
	void StartTransfer_down_seed(unsigned int seed1, unsigned int seed2, unsigned int blk_count);
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

