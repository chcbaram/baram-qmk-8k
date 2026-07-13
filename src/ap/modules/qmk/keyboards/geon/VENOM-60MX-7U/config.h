#pragma once


#define KBD_NAME                    "VENOM-60MX-7U "

#define USB_VID                     0x0483
#define USB_PID                     0x5204


// hw_def.h
//
// #define _USE_HW_VCOM
#define _USE_HW_WS2812
#define     HW_WS2812_MAX_CH        1


// eeprom
//
#define EEPROM_CHIP_ZD24C128 
#define EECONFIG_USER_DATA_SIZE     512               
#define TOTAL_EEPROM_BYTE_COUNT     4096

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

#define MATRIX_ROWS                 5
#define MATRIX_COLS                 16

// GPIO+DMA 로우 캡처(mkey) 시험: PSSI 대신 TIM3+GPDMA로 GPIOC->IDR 캡처, 프레임 자가치유.
#define USE_MKEY_SCAN
// 샘플 지연 CCR2(TIM3 count, 6.25ns): 창 중앙 50%=64(전환경계서 최대 이격). 90%=115은 다중키 고스팅.
#define MKEY_SAMPLE_CCR             64
// 캡처 slot→컬럼 정적 정렬 보정(순환). init 1회 동기 위상 기준: 물리8(콤마)→out4(v) → 7.
#define MKEY_COL_SHIFT             7

#define DEBOUNCE                    20


// #define DEBUG_KEY_SEND
#define DEBUG_MATRIX_SCAN_RATE
#define GRAVE_ESC_ENABLE
#define KILL_SWITCH_ENABLE
#define KKUK_ENABLE