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
	u8 buf[32] = {0};
	u8 comd = 0x1;
	u8 *p = &buf[0];
	int rev, i;

	/* 激活第一个喷头 */
//	ph_uart.ActivePh0();

	/* 激活第二个喷头 */
	ph_uart.ActivePh1();	

	/* 发送读serial number命令然后接收返回数据 */
	rev = ph_uart.PhTransmitCommand_ReviceData(comd, p);

	/* 打印返回数据 */
	for (i = 0; i < rev; i++)
		printf("p[%d] -- %x\n", i, p[i]);

	return 0;
}



