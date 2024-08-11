#pragma once


#define KBD_NAME                    "BARAM-53K-U5-K"

#define USB_VID                     0x0483
#define USB_PID                     0x5203


// hw_def.h
//
#define _USE_HW_VCOM
#define _USE_HW_WS2812
#define     HW_WS2812_MAX_CH        53

#define _USE_HW_ENCODER
#define      HW_ENCODER_MAX_CH      1


// eeprom
//
#define EEPROM_CHIP_ZD24C128 
#define EECONFIG_USER_DATA_SIZE     512
#define TOTAL_EEPROM_BYTE_COUNT     4096

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

#define MATRIX_ROWS                 8
#define MATRIX_COLS                 8

#define DEBOUNCE                   10

#define DEBUG_MATRIX_SCAN_RATE
#define GRAVE_ESC_ENABLE
// #define KKUK_ENABLE
// #define KEY_SEND_DEBUG


// RGB LIGHT : set(RGBLIGHT_ENABLE true)
// 
#define EEPROM_ENABLE
#define RGBLIGHT_SLEEP
#define RGBLIGHT_DEFAULT_ON         false
#define RGBLIGHT_DEFAULT_HUE        0
#define RGBLIGHT_DEFAULT_SAT        0
#define RGBLIGHT_DEFAULT_VAL        128
#define RGBLIGHT_LED_COUNT          68
#define RGBLIGHT_LIMIT_VAL          100
#define RGBLIGHT_SAT_STEP           8
#define RGBLIGHT_VAL_STEP           8
#define RGBLIGHT_EFFECT_RGB_TEST
#define RGBLIGHT_EFFECT_BREATHING
#define RGBLIGHT_EFFECT_SNAKE
#define RGBLIGHT_EFFECT_STATIC_GRADIENT