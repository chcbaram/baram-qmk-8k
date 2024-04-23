#ifndef TOUCH_H_
#define TOUCH_H_


#ifdef __cplusplus
 extern "C" {
#endif


#include "hw_def.h"



#ifdef _USE_HW_TOUCH


#define TOUCH_MAX_CH  HW_TOUCH_MAX_CH


typedef struct
{
  uint8_t  event;
  uint8_t  id;
  int16_t  x;
  int16_t  y;
  int16_t  w;
} touch_point_t;

typedef struct 
{
  uint8_t count;
  touch_point_t point[TOUCH_MAX_CH];
} touch_info_t;




bool touchInit(void);
bool touchClear(void);
bool touchGetInfo(touch_info_t *p_info);
bool touchSetEnable(bool enable);
bool touchGetEnable(void);

#endif


#ifdef __cplusplus
}
#endif


#endif 