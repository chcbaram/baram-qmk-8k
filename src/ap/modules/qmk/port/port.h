
#pragma once


#include "via_hid.h"
#include "via.h"
#include "eeconfig.h"




#define QMK_BUILDDATE   "2024-04-23-11:29:54"


#define EECONFIG_USER_LED_CAPS        ((void *)((uint32_t)EECONFIG_USER_DATABLOCK + 0))
#define EECONFIG_USER_LED_SCROLL      ((void *)((uint32_t)EECONFIG_USER_DATABLOCK + 4))