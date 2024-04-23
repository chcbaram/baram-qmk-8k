#ifndef HW_DEF_H_
#define HW_DEF_H_



#include "bsp.h"


#define _DEF_FIRMWATRE_VERSION    "V240421R1"
#define _DEF_BOARD_NAME           "BARAM-45K-HS-FW"


#define _USE_HW_MICROS

#define _USE_HW_LED
#define      HW_LED_MAX_CH          3

#define _USE_HW_UART
#define      HW_UART_MAX_CH         2
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_USB         _DEF_UART2
#define      HW_UART_CH_CLI         HW_UART_CH_SWD

#define _USE_HW_BUTTON
#define      HW_BUTTON_MAX_CH       2

#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    32
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    8
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_CLI_GUI
#define      HW_CLI_GUI_WIDTH       80
#define      HW_CLI_GUI_HEIGHT      24

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_SWD
#define      HW_LOG_BOOT_BUF_MAX    2048
#define      HW_LOG_LIST_BUF_MAX    4096

#define _USE_HW_USB
#define _USE_HW_CDC
#define      HW_USB_LOG             1
#define      HW_USB_CMP             1
#define      HW_USB_CDC             1
#define      HW_USB_MSC             0
#define      HW_USB_HID             1

#define _USE_HW_KEYS
#define      HW_KEYS_MAX_CH         HW_BUTTON_MAX_CH
#define      HW_KEYS_PRESS_MAX      6

#define _USE_HW_SPI
#define      HW_SPI_MAX_CH          1

#define _USE_HW_EEPROM
#define      HW_EEPROM_MAX_PAGES    32
#define      HW_EEPROM_MODE         0



//-- CLI
//
#define _USE_CLI_HW_BUTTON          1
#define _USE_CLI_HW_KEYSCAN         1
#define _USE_CLI_HW_EEPROM          1

#endif
