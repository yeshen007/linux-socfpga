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


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


static CYxmIntf yxm;




int main(void)
{	
	unsigned int  addr;
	unsigned short val;

   	yxm.Init();

	yxm.InitHole();

	/* 喷头1的孔初始值 */
	for (addr = 0; addr < 332; addr++) {
		val = yxm.ReadHole(addr, 0);
		printf("init ph 0 addr 0x%u val 0x%x\n", addr, val);
	}	

	/* 喷头2的孔初始值 */
	for (addr = 0; addr < 332; addr++) {
		val = yxm.ReadHole(addr, 1);
		printf("init ph 1 addr 0x%u val 0x%x\n", addr, val);
	}


	/* 喷头1的孔全部0xff00 */
	for (addr = 0; addr < 332; addr++) {
		yxm.WriteHole(addr, 0xff00, 0);
		val = yxm.ReadHole(addr, 0);
		printf("ph 0 addr 0x%u val 0x%x\n", addr, val);
	}

	/* 喷头2的孔全部0xcc33 */
	for (addr = 0; addr < 332; addr++) {
		yxm.WriteHole(addr, 0xcc33, 1);
		val = yxm.ReadHole(addr, 1);
		printf("ph 1 addr 0x%u val 0x%x\n", addr, val);
	}	

	yxm.Close();	

	return 0;
}


