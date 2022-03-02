
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "utils.h"

//TAP32 = 32'B10000000000000000000000001100010
#define MASK32_2    0x00000002
#define MASK32_6    0x00000020
#define MASK32_7    0x00000040
#define MASK32_32   0x80000000
#define TAP32       0x80000062
//#define TAB16 0x8016

#if 0
uint32_t ReducationXor_0_31(uint32_t operand)
{
    uint32_t mask = 1;
    uint32_t result = operand & mask;
    for(int i = 0;i < 31; i++)
    {
        operand>>=1;
        result = (operand & mask) ^ result ;
    }

    return result;
}

uint32_t ReducationNor_0_30(uint32_t operand)
{
    uint32_t mod = 1;
    uint32_t result = operand & mod;
    for(int i = 0;i < 30; i++)
    {
        operand>>=1;
        result = ((operand & mod) || result );
    }
    result = !result;
    
    return result;
}
#else
// BIT XOR
uint32_t ReducationXor_0_31(uint32_t operand)
{
    uint32_t bit_2 = (operand>>1);
    uint32_t bit_6 = (operand>>5);
    uint32_t bit_7 = (operand>>6);
    uint32_t bit_32 = (operand>>31);

    uint32_t result = (bit_2 ^ bit_6 ^ bit_7 ^ bit_32) & 0x01;  // 也可最低位求和判断奇偶数
    return result;
}

uint32_t ReducationNor_0_30(uint32_t operand)
{
    uint32_t mask = 0x7FFFFFFF;  // low 31
    uint32_t value = operand & mask;
    uint32_t result = !value;

    return result;
}
#endif


uint32_t GetData0(uint32_t seed)
{
#ifdef no_zero
    return ReducationXor_0_31(seed & TAP32);
#else
    uint32_t data0,tempa,tempb;
    tempa = ReducationXor_0_31(seed & TAP32);
    tempb = ReducationNor_0_30(seed);
    data0 = tempa ^ tempb;
    
    //printf ("==tempa/b==%u:%u=====\n", tempa,tempb);

    return data0;
#endif
}

uint32_t GetData31(uint32_t operand)
{
    operand = operand << 1;
    return operand;
}

uint32_t GenPrbsNum(uint32_t seed)
{
    uint32_t data0,data31;
    data31 = GetData31(seed);
    data0  = GetData0(seed);

    //seed 高31位，data0低1位
    return data31| data0;
}

void GenPrbsArray(uint32_t seed, uint32_t channel, uint32_t *pDataBuf, int bufsize)
{
	uint32_t *pBuf = NULL;
	for(int i = 0; i < (int)channel; i++)
	{
		*(pDataBuf+i) = GenPrbsNum(seed +i);
	}

	for(int j = channel; j < bufsize; j++)
	{
		pBuf = pDataBuf + (j-channel);
		*(pBuf + channel ) = GenPrbsNum(*pBuf);
	}		
}

void GenPrbsArray_fix(uint32_t seed, uint32_t channel, uint32_t *pDataBuf, int bufsize)
{
	*pDataBuf = seed;

	uint32_t *pBuf = NULL;
	for(int i = 1; i < (int)channel + 1; i++)
	{
		*(pDataBuf+i) = GenPrbsNum(seed + i - 1);
	}

	for(int j = channel + 1; j < bufsize; j++)
	{
		pBuf = pDataBuf + (j-channel);
		*(pBuf + channel ) = GenPrbsNum(*pBuf);
	}
}   
