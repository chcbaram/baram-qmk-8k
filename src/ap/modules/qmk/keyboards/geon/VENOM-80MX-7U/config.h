#pragma once


#define KBD_NAME                    "VENOM-80MX-7U "

#define USB_VID                     0x0483
#define USB_PID                     0x5207


// hw_def.h
//
#define _USE_HW_WS2812
#define     HW_WS2812_MAX_CH        2


// eeprom
//
#define EEPROM_CHIP_ZD24C128 
#define EECONFIG_USER_DATA_SIZE     512               
#define TOTAL_EEPROM_BYTE_COUNT     4096

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

#define MATRIX_ROWS                 6
#define MATRIX_COLS                 16

#define DEBOUNCE                    20

#define DEBUG_MATRIX_SCAN_RATE
#define GRAVE_ESC_ENABLE
#define KILL_SWITCH_ENABLE
#define KKUK_ENABLE