#ifndef FT6236_H_
#define FT6236_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_FT6236



#define FT6236_REG_DEV_MODE                0x00
#define FT6236_REG_GEST_ID                 0x01
#define FT6236_REG_TD_STATUS               0x02
#define FT6236_REG_P_XH                    0x03
#define FT6236_REG_P_XL                    0x04
#define FT6236_REG_P_YH                    0x05
#define FT6236_REG_P_YL                    0x06
#define FT6236_REG_P_WEIGHT                0x07
#define FT6236_REG_P_MISC                  0x08
#define FT6236_REG_TH_GROUP                0x80
#define FT6236_REG_TH_DIFF                 0x85
#define FT6236_REG_CTRL                    0x86
#define FT6236_REG_TIME_ENTER_MONITOR      0x87
#define FT6236_REG_PERIOID_ACTIVE          0x88
#define FT6236_REG_PERIOID_MONITOR         0x89
#define FT6236_REG_RADIAN_VALUE            0x91
#define FT6236_REG_OFFSET_LEFT_RIGHT       0x92
#define FT6236_REG_OFFSET_UP_DOWN          0x93
#define FT6236_REG_DISTANCE_LEFT_RIGHT     0x94
#define FT6236_REG_DISTANCE_UP_DOWN        0x95
#define FT6236_REG_DISTANCE_ZOOM           0x96
#define FT6236_REG_LIB_VER_H               0xA1
#define FT6236_REG_LIB_VER_L               0xA2
#define FT6236_REG_CIPHER                  0xA3
#define FT6236_REG_G_MODE                  0xA4
#define FT6236_REG_PWR_MODE                0xA5
#define FT6236_REG_FIRMID                  0xA6
#define FT6236_REG_FOCALTECH_ID            0xA8
#define FT6236_REG_STATE                   0xBC




typedef struct
{
  uint8_t  id;
  uint8_t  event;
  uint16_t x;
  uint16_t y;
  uint8_t  weight;
  uint8_t  area;
} ft6236_point_t;

typedef struct
{
  uint8_t gest_id;
  uint8_t count;
  ft6236_point_t point[2];
} ft6236_info_t;


bool ft6236Init(void);
bool ft6236GetInfo(ft6236_info_t *p_info);
uint16_t ft6236GetWidth(void);
uint16_t ft6236GetHeight(void);

#endif

#ifdef __cplusplus
 }
#endif

#endif