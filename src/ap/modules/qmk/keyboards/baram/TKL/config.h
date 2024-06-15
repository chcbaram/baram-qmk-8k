#pragma once


#define KBD_NAME                    "BARAM-8K-TKL"

#define USB_VID                     0x0483
#define USB_PID                     0x5205

#define EEPROM_CHIP_ZD24C128 
#define EECONFIG_USER_DATA_SIZE     512               
#define TOTAL_EEPROM_BYTE_COUNT     4096

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

#define MATRIX_ROWS                 6
#define MATRIX_COLS                 16

#define DEBOUNCE                    10

#define DEBUG_MATRIX_SCAN_RATE
#define GRAVE_ESC_ENABLE