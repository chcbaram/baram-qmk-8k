#ifndef BSP_H_
#define BSP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "def.h"


#include "stm32u5xx_hal.h"



void logPrintf(const char *fmt, ...);



bool bspInit(void);

void delay(uint32_t time_ms);
uint32_t millis(void);
uint32_t micros(void);

void Error_Handler(void);


#ifdef __cplusplus
}
#endif

#endif