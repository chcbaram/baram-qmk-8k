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

// GPIO+DMA 로우 캡처(mkey) 시험 — 80MX는 6-row라 dual-port(GPIOC row0~4 + GPIOB PB6=row5).
#define USE_MKEY_SCAN
// 샘플 지연 CCR2: 창 중앙 50%=64. 60%=77,75%=96.
#define MKEY_SAMPLE_CCR             64
// 정적 정렬 보정(순환): 실측 후 확정. 물리P→outM 관측 → (기존+M-P)%16. 우선 기본.
#define MKEY_COL_SHIFT              7

#define DEBOUNCE                    20

#define DEBUG_MATRIX_SCAN_RATE
#define GRAVE_ESC_ENABLE
#define KILL_SWITCH_ENABLE
#define KKUK_ENABLE