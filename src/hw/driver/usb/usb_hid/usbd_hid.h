/**
  ******************************************************************************
  * @file    usbd_hid.h
  * @author  MCD Application Team
  * @brief   Header file for the usbd_hid_core.c file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_HID_H
#define __USB_HID_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include  "usbd_ioreq.h"

/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_HID
  * @brief This file is the Header file for usbd_hid.c
  * @{
  */


/** @defgroup USBD_HID_Exported_Defines
  * @{
  */
#define HID_EP_SIZE                                     64U

#define HID_EPIN_ADDR                                   0x81U
#define HID_EPIN_SIZE                                   64U

#define HID_VIA_EP_IN                                   0x84U
#define HID_VIA_EP_OUT                                  0x04U
#define HID_VIA_EP_SIZE                                 32U

#define HID_EXK_EP_IN                                   0x85U
#define HID_EXK_EP_SIZE                                 8U

#define HID_KEYBOARD
#define USB_HID_CONFIG_DESC_SIZ                         91U  
#define USB_HID_DESC_SIZ                                9U

#define HID_MOUSE_REPORT_DESC_SIZE                      74U
#define HID_KEYBOARD_REPORT_DESC_SIZE                   64U
#define HID_KEYBOARD_VIA_REPORT_DESC_SIZE               34U
#define HID_EXK_REPORT_DESC_SIZE                        50U

#define HID_DESCRIPTOR_TYPE                             0x21U
#define HID_REPORT_DESC                                 0x22U

#define HID_HS_BINTERVAL                                0x01U
#define HID_FS_BINTERVAL                                0x01U

#define USBD_HID_REQ_SET_PROTOCOL                       0x0BU
#define USBD_HID_REQ_GET_PROTOCOL                       0x03U

#define USBD_HID_REQ_SET_IDLE                           0x0AU
#define USBD_HID_REQ_GET_IDLE                           0x02U

#define USBD_HID_REQ_SET_REPORT                         0x09U
#define USBD_HID_REQ_GET_REPORT                         0x01U


/**
  * @}
  */


/** @defgroup USBD_CORE_Exported_TypesDefinitions
  * @{
  */
typedef enum
{
  USBD_HID_IDLE = 0,
  USBD_HID_BUSY,
} USBD_HID_StateTypeDef;


typedef struct
{
  uint32_t Protocol;
  uint32_t IdleState;
  uint32_t AltSetting;
  USBD_HID_StateTypeDef state;
} USBD_HID_HandleTypeDef;

/*
 * HID Class specification version 1.1
 * 6.2.1 HID Descriptor
 */

typedef struct
{
  uint8_t           bLength;
  uint8_t           bDescriptorType;
  uint16_t          bcdHID;
  uint8_t           bCountryCode;
  uint8_t           bNumDescriptors;
  uint8_t           bHIDDescriptorType;
  uint16_t          wItemLength;
} __PACKED USBD_HIDDescTypeDef;

/**
  * @}
  */



/** @defgroup USBD_CORE_Exported_Macros
  * @{
  */

/**
  * @}
  */

/** @defgroup USBD_CORE_Exported_Variables
  * @{
  */

extern USBD_ClassTypeDef USBD_HID;
#define USBD_HID_CLASS &USBD_HID
/**
  * @}
  */

/** @defgroup USB_CORE_Exported_Functions
  * @{
  */
uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev);


enum 
{
  USB_HID_LED_NUM_LOCK    = (1 << 0),
  USB_HID_LED_CAPS_LOCK   = (1 << 1),
  USB_HID_LED_SCROLL_LOCK = (1 << 2),
  USB_HID_LED_COMPOSE     = (1 << 3),
  USB_HID_LED_KANA        = (1 << 4)
};

typedef struct
{
  uint32_t freq_hz;
  uint32_t time_max;
  uint32_t time_min;
} usb_hid_rate_info_t;

bool usbHidSetViaReceiveFunc(void (*func)(uint8_t *, uint8_t));
bool usbHidSendReport(uint8_t *p_data, uint16_t length);
bool usbHidSendReportEXK(uint8_t *p_data, uint16_t length);
bool usbHidGetRateInfo(usb_hid_rate_info_t *p_info);
bool usbHidSetTimeLog(uint16_t index, uint32_t time_us);
void usbHidSetStatusLed(uint8_t led_bits);

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif  /* __USB_HID_H */
/**
  * @}
  */

/**
  * @}
  */

