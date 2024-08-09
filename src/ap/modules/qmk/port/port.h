
#pragma once


#include "via_hid.h"
#include "via.h"
#include "eeconfig.h"
#include "kill_switch.h"
#include "kkuk.h"



#define QMK_BUILDDATE   "2024-04-23-11:29:54"


#define EECONFIG_USER_LED_CAPS        ((void *)((uint32_t)EECONFIG_USER_DATABLOCK +  0)) // 4B
#define EECONFIG_USER_LED_SCROLL      ((void *)((uint32_t)EECONFIG_USER_DATABLOCK +  4)) // 4B
#define EECONFIG_USER_KILL_SWITCH_LR  ((void *)((uint32_t)EECONFIG_USER_DATABLOCK +  8)) // 8B
#define EECONFIG_USER_KILL_SWITCH_UD  ((void *)((uint32_t)EECONFIG_USER_DATABLOCK + 16)) // 8B
#define EECONFIG_USER_KKUK            ((void *)((uint32_t)EECONFIG_USER_DATABLOCK + 24)) // 4B