#ifndef SDRAM_H_
#define SDRAM_H_


#ifdef __cplusplus
 extern "C" {
#endif


#include "hw_def.h"

#ifdef _USE_HW_SDRAM


#define SDRAM_MEM_ADDR    HW_SDRAM_MEM_ADDR
#define SDRAM_MEM_SIZE    HW_SDRAM_MEM_SIZE


bool sdramInit(void);
bool sdramIsInit(void);

uint32_t sdramGetAddr(void);
uint32_t sdramGetLength(void);

#endif

#ifdef __cplusplus
}
#endif


#endif