#ifndef st7701_H_
#define st7701_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_ST7701
#include "lcd.h"
#include "st7701_regs.h"



#define ST7701_WIDTH     HW_ST7701_WIDTH
#define ST7701_HEIGHT    HW_ST7701_HEIGHT



bool st7701Init(void);
bool st7701SetRotate(bool enable);

lcd_driver_t *st7701GetDriver(void);


#endif

#ifdef __cplusplus
}
#endif

#endif