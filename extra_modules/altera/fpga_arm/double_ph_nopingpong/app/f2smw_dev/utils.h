#ifndef __F2SM_UTILS_H__
#define __F2SM_UTILS_H__

#include <stdint.h>

uint32_t GenPrbsNum(uint32_t);
void GenPrbsArray(uint32_t seed, uint32_t channel, uint32_t *pDataBuf, int bufsize);
void GenPrbsArray_fix(uint32_t seed, uint32_t channel, uint32_t *pDataBuf, int bufsize);

#endif
