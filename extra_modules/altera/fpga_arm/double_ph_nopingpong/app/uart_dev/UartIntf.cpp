#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "UartIntf.h"
#include "alt_f2sm_regs.h"

#define UART0_BRIDGE_OFFSET 0x800
#define UART1_BRIDGE_OFFSET 0x820


#define RXDATA_OFFSET 		0
#define TXDATA_OFFSET 		1
#define STATUS_OFFSET 		2
#define CONTROL_OFFSET 		3
#define DIVISOR_OFFSET 		4
#define ENDOFPACKET_OFFSET 	5

#define CLOCK_FREQ 1000000		//暂时随便给让编译通过	


#define DEBUG
 
#ifdef DEBUG
	#define debug(format...) printf(format)
#else
	#define debug(format...) 
#endif


/* 
 * 父类
 */
CUartIntf::CUartIntf()
{
	int fd;
	void *h2f_brige_virtual_base;
	
	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		fprintf(stderr, "Open /dev/mem failed!\n");
		exit(-1);
    }

	h2f_brige_virtual_base = mmap(NULL, MMAP_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, MMAP_BASE);
	if (h2f_brige_virtual_base == MAP_FAILED) {
		fprintf(stderr, "Mmap regs failed!\n");
		close(fd);
		exit(-1);
	}

	ph0_uart_virtual_base = (u8 *)h2f_brige_virtual_base + UART0_BRIDGE_OFFSET;
	ph1_uart_virtual_base =	(u8 *)h2f_brige_virtual_base + UART1_BRIDGE_OFFSET;
	active_virtual_base = NULL;

	close(fd);
}

CUartIntf::~CUartIntf()
{
	void *h2f_brige_virtual_base;

	h2f_brige_virtual_base = (u8 *)ph0_uart_virtual_base - UART0_BRIDGE_OFFSET;
	munmap(h2f_brige_virtual_base, MMAP_SPAN);
	
	ph0_uart_virtual_base = NULL;
	ph1_uart_virtual_base = NULL;
	active_virtual_base = NULL;
}


u8 CUartIntf::ReviceData(void)
{
	void *reg_addr = NULL;
	u16 reg_val;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, RXDATA_OFFSET);
	reg_val = alt_read_half_word(reg_addr);
	
	return (u8)reg_val;
}

u8 CUartIntf::ReviceDataPoll(void)
{
	void *reg_addr = NULL;
	u16 reg_val;
	
	void *status_addr = NULL;
	u16 status_val;
	u16 rrdy_mask = 0x80;

	status_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, STATUS_OFFSET);
	
	/* 轮询直到rrdy为1 */
	do {
		status_val = alt_read_half_word(status_addr);
		status_val &= rrdy_mask;
	} while (!status_val);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, RXDATA_OFFSET);
	reg_val = alt_read_half_word(reg_addr);

	debug("rev one byte\n");
	
	return (u8)reg_val;	

}


u16 CUartIntf::GetStatus(void)
{
	void *reg_addr = NULL;
	u16 reg_val;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, STATUS_OFFSET);
	reg_val = alt_read_half_word(reg_addr);
	reg_val &= 0x1fff;
	
	return reg_val;
}


u16 CUartIntf::GetControl(void)
{
	void *reg_addr = NULL;
	u16 reg_val;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, CONTROL_OFFSET);
	reg_val = alt_read_half_word(reg_addr);
	reg_val &= 0x1fff;
	
	return reg_val;
}

u16 CUartIntf::GetDivisor(void)
{
	void *reg_addr = NULL;
	u16 reg_val;
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, DIVISOR_OFFSET);
	reg_val = alt_read_half_word(reg_addr);	
	
	return reg_val;
}

u32 CUartIntf::GetBaud(void)
{
	void *divisor_addr = NULL;
	u16 divisor;
	u32 baud;
	
	divisor_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, DIVISOR_OFFSET);
	divisor = alt_read_half_word(divisor_addr);
	baud = CLOCK_FREQ / ((u32)divisor + 1);
	
	return baud;
}

u8 CUartIntf::GetEndOfPacket(void)
{
	void *reg_addr = NULL;
	u16 reg_val;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, ENDOFPACKET_OFFSET);
	reg_val = alt_read_half_word(reg_addr);	
	
	return (u8)reg_val;
}

void CUartIntf::TransmitData(u8 data)
{
	void *reg_addr = NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, TXDATA_OFFSET);
	alt_write_half_word(reg_addr, (u16)data);
}


void CUartIntf::TransmitDataPoll(u8 data)
{
	void *reg_addr = NULL;

	void *status_addr = NULL;
	u16 status_val;
	u16 tmt_trdy_mask = 0x60;

	status_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, STATUS_OFFSET);

	/* 轮询直到TMT为1且TRDY为1 */
	do {
		status_val = alt_read_half_word(status_addr);
		status_val &= tmt_trdy_mask;
	} while (status_val != tmt_trdy_mask);

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, TXDATA_OFFSET);
	alt_write_half_word(reg_addr, (u16)data);

}


void CUartIntf::ClearStatus(void)
{
	void *reg_addr = NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, STATUS_OFFSET);
	alt_write_half_word(reg_addr, 0);
}

void CUartIntf::SetControl(u16 value)
{
	void *reg_addr = NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, CONTROL_OFFSET);
	alt_write_half_word(reg_addr, value);
}

void CUartIntf::SetDivisor(u16 divisor)
{
	void *reg_addr = NULL;

	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, DIVISOR_OFFSET);
	alt_write_half_word(reg_addr, divisor);
}

void CUartIntf::SetBaud(u32 baud)
{
	void *reg_addr = NULL;
	u16 divisor;

	divisor = (u16)(CLOCK_FREQ / baud - 1);
	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, DIVISOR_OFFSET);
	alt_write_half_word(reg_addr, divisor);
}

void CUartIntf::SetEndOfPacket(u8 endofpacket)
{
	void *reg_addr = NULL;
	
	reg_addr = __IO_CALC_ADDRESS_NATIVE(active_virtual_base, ENDOFPACKET_OFFSET);
	alt_write_half_word(reg_addr, (u16)endofpacket);
}


int CUartIntf::CheckError(u16 mask)
{
	//检查PE,FE,BPK,ROE,TOE,E,DCTS是否错误
	return 0;
}



/* 
 * 子类
 */
CPhUartIntf::CPhUartIntf()
{
	
}

CPhUartIntf::~CPhUartIntf()
{
	
}

void CPhUartIntf::ActivePh0(void)
{
	active_virtual_base = ph0_uart_virtual_base;
	ClearStatus();
}

void CPhUartIntf::ActivePh1(void)
{
	active_virtual_base = ph1_uart_virtual_base;
	ClearStatus();
}


void CPhUartIntf::PhTransmitCommand(u8 comd)
{
	switch (comd) {
	case 0x01:
		TransmitDataPoll(0xFE);
		TransmitDataPoll(0xFE);
		TransmitDataPoll(0xFE);
		TransmitDataPoll(4);		//byte number + 2
		TransmitDataPoll(0x1);		//command code 
		TransmitDataPoll(0);		//data 1
		TransmitDataPoll(0);		//data 2
		TransmitDataPoll(0xFB);		//sum
		break;
	default:
		printf("ready to do..\n");
		break;
	}

}

/* 返回收到的数据量 */
int CPhUartIntf::PhReviceData(u8 comd, void *pdata)
{
	int i = 0;
	u8 *p = (u8 *)pdata;
	
	switch (comd) {
	case 0x01:
		for (i = 0; i < 14; i++) 
			*(p + i) = ReviceDataPoll();
		break;
	default:
		printf("ready to do...\n");
		break;
	}

	return i;

}

int CPhUartIntf::PhTransmitCommand_ReviceData(u8 comd, void *pdata)
{
	int ret = 0;
	
	/* 先发送命令 */
	PhTransmitCommand(comd);

	debug("transmit command done\n");

	/* 再接收数据 */
	ret = PhReviceData(comd, pdata);

	return ret;
}



