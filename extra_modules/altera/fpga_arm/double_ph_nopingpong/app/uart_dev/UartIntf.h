#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


/* public: 类中、子类和类外的实例对象都能访问
 * protected: 类外实例对象不能访问，类中和子类可以访问
 * private: 只有类中能访问
 */
class CUartIntf
{
public:
    void *ph0_uart_virtual_base;				//喷头0串口寄存器基地址
	void *ph1_uart_virtual_base;				//喷头1串口寄存器基地址
	void *active_virtual_base;					//正在操作的串口寄存器基地址
	
    CUartIntf();								//设置ph0_uart_virtual_base和ph1_uart_virtual_base
	~CUartIntf();								//销毁ph0_uart_virtual_base,ph1_uart_virtual_base,active_virtual_base
	u8 ReviceData(void);						//读取rxdata
	u8 ReviceDataPoll(void);					//等到RRDY为1再读取rxdata
	u16 GetStatus(void);
	u16 GetControl(void);
	u16 GetDivisor(void);						//硬件不支持,不能用
	u32 GetBaud(void);							//硬件不支持,不能用
	u8 GetEndOfPacket(void);					//硬件不支持,不能用
	void TransmitData(u8 data);					//写数据到txdata
	void TransmitDataPoll(u8 data);				//等到TMT为1且TRDY为1再写数据到txdata
	void ClearStatus(void);
	void SetControl(u16 value);
	void SetDivisor(u16 divisor);				//硬件不支持,不能用
	void SetBaud(u32 baud);						//硬件不支持,不能用
	void SetEndOfPacket(u8 endofpacket);		//硬件不支持,不能用
	int CheckError(u16 mask);					
};



/*
----------+-----------+-----------+---------+
父类属性      | public    | protected | private |
继承方式\     |           |           |         |
----------+-----------+-----------+---------+
public    | public    | protected | 隔离      |
----------+-----------+-----------+---------+
protected | protected | protected | 隔离    |
----------+-----------+-----------+---------+
private   | private   | private   | 隔离      |
----------+-----------+-----------+---------+
*
* 上面的表说明了在不同的继承方式下父类中的各种属性的成员在子类中变成相应的属性.
* 值得注意的是父类的private无论以什么方式被继承都不能被子类实例化对象和子类直接访问,
* 因此唯一的访问方式是通过继承父类的public和protected成员函数来间接访问.
*/
class CPhUartIntf : public CUartIntf
{
public:
    CPhUartIntf();						
	~CPhUartIntf();						
	void ActivePh0(void);				//激活ph0,即设置active_virtual_base = ph0_uart_virtual_base
	void ActivePh1(void);				//激活ph1,即设置active_virtual_base = ph1_uart_virtual_base	

	/* 发送命令comd给喷头 
     * 接收喷头发送的数据
     * 先发送命令comd给喷头,然后等待接收喷头发回的响应数据
	 */
	void PhTransmitCommand(u8 comd);
	int PhReviceData(void *pdata);	
	int PhTransmitCommand_ReviceData(u8 comd, void *pdata);
};

