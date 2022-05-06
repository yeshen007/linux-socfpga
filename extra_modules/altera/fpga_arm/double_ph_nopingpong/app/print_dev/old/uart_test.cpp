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
#include "UartIntf.h"



static CPhUartIntf ph_uart;

int main(void)
{		
	//u32  reg_addr;
	//u16  reg_val;
	
	ph_uart.ActivePh0();


	//printf("uart0 base is:%x\n", reg_addr);

	return 0;
}



