/**
  ******************************************************************************
  * @file    usbd_hid.c
  * @author  MCD Application Team
  * @brief   This file provides the HID core functions.
  *
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
  * @verbatim
  *
  *          ===================================================================
  *                                HID Class  Description
  *          ===================================================================
  *           This module manages the HID class V1.11 following the "Device Class Definition
  *           for Human Interface Devices (HID) Version 1.11 Jun 27, 2001".
  *           This driver implements the following aspects of the specification:
  *             - The Boot Interface Subclass
  *             - The Mouse protocol
  *             - Usage Page : Generic Desktop
  *             - Usage : Joystick
  *             - Collection : Application
  *
  * @note     In HS mode and when the DMA is used, all variables and data structures
  *           dealing with the DMA during the transaction process should be 32-bit aligned.
  *
  *
  *  @endverbatim
  *
  ******************************************************************************
  */


#include "usbd_hid.h"
#include "usbd_ctlreq.h"
#include "usbd_desc.h"

#include "cli.h"
#include "log.h"
#include "keys.h"
#include "qbuffer.h"
#include "report.h"


#if HW_USB_LOG == 1
#define logDebug(...)                              \
  {                                                \
    if (HW_LOG_CH == HW_UART_CH_USB) logDisable(); \
    logPrintf(__VA_ARGS__);                        \
    if (HW_LOG_CH == HW_UART_CH_USB) logEnable();  \
  }
#else
#define logDebug(...) 
#endif


#define HID_KEYBOARD_REPORT_SIZE (HW_KEYS_PRESS_MAX + 2U)
#define KEY_TIME_LOG_MAX         32


static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_HID_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_HID_SOF(USBD_HandleTypeDef *pdev);

#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);
#endif /* USE_USBD_COMPOSITE  */

#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
static uint8_t *USBD_HID_GetUsrStrDescriptor(struct _USBD_HandleTypeDef *pdev, uint8_t index,  uint16_t *length);
#endif


static void cliCmd(cli_args_t *args);
static void usbHidMeasurePollRate(void);
static void usbHidMeasureRateTime(void);
static bool usbHidUpdateWakeUp(USBD_HandleTypeDef *pdev);
static void usbHidInitTimer(void);





typedef struct
{
  uint8_t  buf[HID_KEYBOARD_REPORT_SIZE];
} report_info_t;

typedef struct
{
  uint8_t  buf[32];
} via_report_info_t;

typedef struct
{
  uint8_t len;
  uint8_t buf[HID_EXK_EP_SIZE];
} exk_report_info_t;

static USBD_SetupReqTypedef ep0_req;
static uint8_t ep0_req_buf[USB_MAX_EP0_SIZE];

static qbuffer_t             via_report_q;
static via_report_info_t     via_report_q_buf[128];
static uint32_t              via_report_pre_time;
static uint32_t              via_report_time = 20;
__ALIGN_BEGIN static uint8_t via_hid_usb_report[32] __ALIGN_END;
static void (*via_hid_receive_func)(uint8_t *data, uint8_t length) = NULL;


static qbuffer_t              report_q;
static report_info_t          report_buf[128];
__ALIGN_BEGIN  static uint8_t hid_buf[HID_KEYBOARD_REPORT_SIZE] __ALIGN_END = {0,};

static qbuffer_t              report_exk_q;
static exk_report_info_t      report_exk_buf[128];
__ALIGN_BEGIN  static uint8_t hid_buf_exk[HID_EXK_EP_SIZE] __ALIGN_END = {0,};



USBD_ClassTypeDef USBD_HID =
{
  USBD_HID_Init,
  USBD_HID_DeInit,
  USBD_HID_Setup,
  NULL,                 /* EP0_TxSent */
  USBD_HID_EP0_RxReady, /* EP0_RxReady */
  USBD_HID_DataIn,      /* DataIn */
  USBD_HID_DataOut,     /* DataOut */
  USBD_HID_SOF,         /* SOF */
  NULL,
  NULL,
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USBD_HID_GetHSCfgDesc,
  USBD_HID_GetFSCfgDesc,
  USBD_HID_GetOtherSpeedCfgDesc,
  USBD_HID_GetDeviceQualifierDesc,
#endif /* USE_USBD_COMPOSITE  */

#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  USBD_HID_GetUsrStrDescriptor,
#endif 
};

#ifndef USE_USBD_COMPOSITE
/* USB HID device FS Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_CfgDesc[USB_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
  0x09,                                               /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,                        /* bDescriptorType: Configuration */
  USB_HID_CONFIG_DESC_SIZ,                            /* wTotalLength: Bytes returned */
  0x00,
  0x03,                                               /* bNumInterfaces: 3 interface */
  0x01,                                               /* bConfigurationValue: Configuration value */
  0x00,                                               /* iConfiguration: Index of string descriptor
                                                         describing the configuration */
#if (USBD_SELF_POWERED == 1U)
  0xE0,                                               /* bmAttributes: Bus Powered according to user configuration */
#else
  0xA0,                                               /* bmAttributes: Bus Powered according to user configuration */
#endif /* USBD_SELF_POWERED */
  USBD_MAX_POWER,                                     /* MaxPower (mA) */

  /************** Descriptor of Keyboard interface ****************/
  /* 09 */
  0x09,                                               /* bLength: Interface Descriptor size */
  USB_DESC_TYPE_INTERFACE,                            /* bDescriptorType: Interface descriptor type */
  0x00,                                               /* bInterfaceNumber: Number of Interface */
  0x00,                                               /* bAlternateSetting: Alternate setting */
  0x01,                                               /* bNumEndpoints */
  0x03,                                               /* bInterfaceClass: HID */
  0x01,                                               /* bInterfaceSubClass : 1=BOOT, 0=no boot */
  0x01,                                               /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
  0,                                                  /* iInterface: Index of string descriptor */
  /******************** Descriptor of Keyboard HID ********************/
  /* 18 */
  0x09,                                               /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE,                                /* bDescriptorType: HID */
  0x11,                                               /* bcdHID: HID Class Spec release number */
  0x01,
  0x00,                                               /* bCountryCode: Hardware target country */
  0x01,                                               /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                                               /* bDescriptorType */
  HID_KEYBOARD_REPORT_DESC_SIZE,                      /* wItemLength: Total length of Report descriptor */
  0x00,
  /******************** Descriptor of Keyboard endpoint ********************/
  /* 27 */
  0x07,                                               /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                             /* bDescriptorType:*/

  HID_EPIN_ADDR,                                      /* bEndpointAddress: Endpoint Address (IN) */
  0x03,                                               /* bmAttributes: Interrupt endpoint */
  HID_EPIN_SIZE,                                      /* wMaxPacketSize: */
  0x00,
  HID_HS_BINTERVAL,                                   /* bInterval: Polling Interval */
  /* 34 */


  /*---------------------------------------------------------------------------*/
  /* VIA interface descriptor */
  0x09,                                               /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_INTERFACE,                            /* bDescriptorType: */
  0x01,                                               /* bInterfaceNumber: Number of Interface */
  0x00,                                               /* bAlternateSetting: Alternate setting */
  0x02,                                               /* bNumEndpoints: Two endpoints used */
  0x03,                                               /* bInterfaceClass: HID */
  0x00,                                               /* bInterfaceSubClass : 1=BOOT, 0=no boot */
  0x00,                                               /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
  0x00,                                               /* iInterface */

  /******************** Descriptor of VIA ********************/
  /* 43 */
  0x09,                                               /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE,                                /* bDescriptorType: HID */
  0x11,                                               /* bcdHID: HID Class Spec release number */
  0x01,
  0x00,                                               /* bCountryCode: Hardware target country */
  0x01,                                               /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                                               /* bDescriptorType */
  HID_KEYBOARD_VIA_REPORT_DESC_SIZE,                  /* wItemLength: Total length of Report descriptor */
  0x00,

  /******************** Descriptor of VIA endpoint ********************/
  /* 52 */
  0x07,                                               /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                             /* bDescriptorType:*/
  HID_VIA_EP_IN,                                      /* bEndpointAddress: Endpoint Address (IN) */
  USBD_EP_TYPE_INTR,                                  /* bmAttributes: Interrupt endpoint */
  HID_VIA_EP_SIZE,                                    /* wMaxPacketSize: */
  0x00,
  4,                                                  /* bInterval: Polling Interval */

  /* 59 */
  0x07,                                               /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                             /* bDescriptorType:*/
  HID_VIA_EP_OUT,                                     /* bEndpointAddress: Endpoint Address (OUT) */
  USBD_EP_TYPE_INTR,                                  /* bmAttributes: Interrupt endpoint */
  HID_VIA_EP_SIZE,                                    /* wMaxPacketSize: */
  0x00,
  4,                                                  /* bInterval: Polling Interval */
  /* 66 */


  /*---------------------------------------------------------------------------*/
  /* EXK interface descriptor */
  0x09,                                               /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_INTERFACE,                            /* bDescriptorType: */
  0x02,                                               /* bInterfaceNumber: Number of Interface */
  0x00,                                               /* bAlternateSetting: Alternate setting */
  0x01,                                               /* bNumEndpoints: One endpoint used */
  0x03,                                               /* bInterfaceClass: HID */
  0x01,                                               /* bInterfaceSubClass : 1=BOOT, 0=no boot */
  0x00,                                               /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
  0x00,                                               /* iInterface */

  /******************** Descriptor of EXK ********************/
  /* 75 */
  0x09,                                               /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE,                                /* bDescriptorType: HID */
  0x11,                                               /* bcdHID: HID Class Spec release number */
  0x01,
  0x00,                                               /* bCountryCode: Hardware target country */
  0x01,                                               /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                                               /* bDescriptorType */
  HID_EXK_REPORT_DESC_SIZE,                           /* wItemLength: Total length of Report descriptor */
  0x00,

  /******************** Descriptor of EXK endpoint ********************/
  /* 84 */
  0x07,                                               /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                             /* bDescriptorType:*/
  HID_EXK_EP_IN,                                      /* bEndpointAddress: Endpoint Address (IN) */
  USBD_EP_TYPE_INTR,                                  /* bmAttributes: Interrupt endpoint */
  HID_EXK_EP_SIZE,                                    /* wMaxPacketSize: */
  0x00,
  HID_HS_BINTERVAL,                                   /* bInterval: Polling Interval */
  /* 91 */

};
#endif /* USE_USBD_COMPOSITE  */

/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ] __ALIGN_END =
{
  /* 18 */
  0x09,                                               /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE,                                /* bDescriptorType: HID */
  0x11,                                               /* bcdHID: HID Class Spec release number */
  0x01,
  0x00,                                               /* bCountryCode: Hardware target country */
  0x01,                                               /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                                               /* bDescriptorType */
  HID_KEYBOARD_REPORT_DESC_SIZE,                      /* wItemLength: Total length of Report descriptor */
  0x00,
};

#ifndef USE_USBD_COMPOSITE
/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};
#endif /* USE_USBD_COMPOSITE  */

#if 0
__ALIGN_BEGIN static uint8_t HID_MOUSE_ReportDesc[HID_MOUSE_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,        /* Usage Page (Generic Desktop Ctrls)     */
  0x09, 0x02,        /* Usage (Mouse)                          */
  0xA1, 0x01,        /* Collection (Application)               */
  0x09, 0x01,        /*   Usage (Pointer)                      */
  0xA1, 0x00,        /*   Collection (Physical)                */
  0x05, 0x09,        /*     Usage Page (Button)                */
  0x19, 0x01,        /*     Usage Minimum (0x01)               */
  0x29, 0x03,        /*     Usage Maximum (0x03)               */
  0x15, 0x00,        /*     Logical Minimum (0)                */
  0x25, 0x01,        /*     Logical Maximum (1)                */
  0x95, 0x03,        /*     Report Count (3)                   */
  0x75, 0x01,        /*     Report Size (1)                    */
  0x81, 0x02,        /*     Input (Data,Var,Abs)               */
  0x95, 0x01,        /*     Report Count (1)                   */
  0x75, 0x05,        /*     Report Size (5)                    */
  0x81, 0x01,        /*     Input (Const,Array,Abs)            */
  0x05, 0x01,        /*     Usage Page (Generic Desktop Ctrls) */
  0x09, 0x30,        /*     Usage (X)                          */
  0x09, 0x31,        /*     Usage (Y)                          */
  0x09, 0x38,        /*     Usage (Wheel)                      */
  0x15, 0x81,        /*     Logical Minimum (-127)             */
  0x25, 0x7F,        /*     Logical Maximum (127)              */
  0x75, 0x08,        /*     Report Size (8)                    */
  0x95, 0x03,        /*     Report Count (3)                   */
  0x81, 0x06,        /*     Input (Data,Var,Rel)               */
  0xC0,              /*   End Collection                       */
  0x09, 0x3C,        /*   Usage (Motion Wakeup)                */
  0x05, 0xFF,        /*   Usage Page (Reserved 0xFF)           */
  0x09, 0x01,        /*   Usage (0x01)                         */
  0x15, 0x00,        /*   Logical Minimum (0)                  */
  0x25, 0x01,        /*   Logical Maximum (1)                  */
  0x75, 0x01,        /*   Report Size (1)                      */
  0x95, 0x02,        /*   Report Count (2)                     */
  0xB1, 0x22,        /*   Feature (Data,Var,Abs,NoWrp)         */
  0x75, 0x06,        /*   Report Size (6)                      */
  0x95, 0x01,        /*   Report Count (1)                     */
  0xB1, 0x01,        /*   Feature (Const,Array,Abs,NoWrp)      */
  0xC0               /* End Collection                         */
};
#endif

__ALIGN_BEGIN static uint8_t HID_KEYBOARD_ReportDesc[HID_KEYBOARD_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,                         // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,                         // USAGE (Keyboard)
  0xa1, 0x01,                         // COLLECTION (Application)
  0x05, 0x07,                         //   USAGE_PAGE (Keyboard)
  0x19, 0xe0,                         //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xe7,                         //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,                         //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                         //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,                         //   REPORT_SIZE (1)
  0x95, 0x08,                         //   REPORT_COUNT (8)
  0x81, 0x02,                         //   INPUT (Data,Var,Abs)
  0x95, 0x01,                         //   REPORT_COUNT (1)
  0x75, 0x08,                         //   REPORT_SIZE (8)
  0x81, 0x03,                         //   INPUT (Cnst,Var,Abs)
  0x95, 0x05,                         //   REPORT_COUNT (5)
  0x75, 0x01,                         //   REPORT_SIZE (1)
  0x05, 0x08,                         //   USAGE_PAGE (LEDs)
  0x19, 0x01,                         //   USAGE_MINIMUM (Num Lock)
  0x29, 0x05,                         //   USAGE_MAXIMUM (Kana)
  0x91, 0x02,                         //   OUTPUT (Data,Var,Abs)
  0x95, 0x01,                         //   REPORT_COUNT (1)
  0x75, 0x03,                         //   REPORT_SIZE (3)
  0x91, 0x03,                         //   OUTPUT (Cnst,Var,Abs)
  0x95, HW_KEYS_PRESS_MAX,            //   REPORT_COUNT (6)
  0x75, 0x08,                         //   REPORT_SIZE (8)
  0x15, 0x00,                         //   LOGICAL_MINIMUM (0)
  0x26, 0xFF, 0x00,                   //   LOGICAL_MAXIMUM (255)
  0x05, 0x07,                         //   USAGE_PAGE (Keyboard)
  0x19, 0x00,                         //   USAGE_MINIMUM (Reserved (no event indicated))
  0x29, 0xFF,                         //   USAGE_MAXIMUM (Keyboard Application)
  0x81, 0x00,                         //   INPUT (Data,Ary,Abs)
  0xc0                                // END_COLLECTION
};

__ALIGN_BEGIN static uint8_t HID_VIA_ReportDesc[HID_KEYBOARD_VIA_REPORT_DESC_SIZE] __ALIGN_END = 
{
  //
  0x06, 0x60, 0xFF, // Usage Page (Vendor Defined)
  0x09, 0x61,       // Usage (Vendor Defined)
  0xA1, 0x01,       // Collection (Application)
  // Data to host
  0x09, 0x62,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 32,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x81, 0x02,       //   Input (Data, Variable, Absolute)
  // Data from host
  0x09, 0x63,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 32,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x91, 0x02,       //   Output (Data, Variable, Absolute)
  0xC0              // End Collection
};

__ALIGN_BEGIN static uint8_t HID_EXK_ReportDesc[HID_EXK_REPORT_DESC_SIZE] __ALIGN_END =
{
  //
  0x05, 0x01,               // Usage Page (Generic Desktop)
  0x09, 0x80,               // Usage (System Control)
  0xA1, 0x01,               // Collection (Application)
  0x85, REPORT_ID_SYSTEM,   //   Report ID
  0x19, 0x01,               //   Usage Minimum (Pointer)
  0x2A, 0xB7, 0x00,         //   Usage Maximum (System Display LCD Autoscale)
  0x15, 0x01,               //   Logical Minimum
  0x26, 0xB7, 0x00,         //   Logical Maximum
  0x95, 0x01,               //   Report Count (1)
  0x75, 0x10,               //   Report Size (16)
  0x81, 0x00,               //   Input (Data, Array, Absolute)
  0xC0,                     // End Collection

  0x05, 0x0C,               // Usage Page (Consumer)
  0x09, 0x01,               // Usage (Consumer Control)
  0xA1, 0x01,               // Collection (Application)
  0x85, REPORT_ID_CONSUMER, //   Report ID
  0x19, 0x01,               //   Usage Minimum (Consumer Control)
  0x2A, 0xA0, 0x02,         //   Usage Maximum (AC Desktop Show All Applications)
  0x15, 0x01,               //   Logical Minimum
  0x26, 0xA0, 0x02,         //   Logical Maximum
  0x95, 0x01,               //   Report Count (1)
  0x75, 0x10,               //   Report Size (16)
  0x81, 0x00,               //   Input (Data, Array, Absolute)
  0xC0                      // End Collection
};

static USBD_HID_HandleTypeDef *p_hhid = NULL;
static uint8_t HIDInEpAdd = HID_EPIN_ADDR;
extern USBD_HandleTypeDef USBD_Device;
static TIM_HandleTypeDef htim2;


/**
  * @brief  USBD_HID_Init
  *         Initialize the HID interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  USBD_HID_HandleTypeDef *hhid;

  hhid = (USBD_HID_HandleTypeDef *)USBD_malloc(sizeof(USBD_HID_HandleTypeDef));

  if (hhid == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  p_hhid = hhid;

  pdev->pClassDataCmsit[pdev->classId] = (void *)hhid;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];


#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this class instance */
  HIDInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */
  pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = pdev->dev_speed == USBD_SPEED_HIGH ? HID_HS_BINTERVAL:HID_FS_BINTERVAL;

  /* Open EP IN */
  (void)USBD_LL_OpenEP(pdev, HIDInEpAdd, USBD_EP_TYPE_INTR, HID_EPIN_SIZE);
  pdev->ep_in[HIDInEpAdd & 0xFU].is_used = 1U;


  // VIA EP
  //
  pdev->ep_in[HID_VIA_EP_IN & 0xFU].bInterval = pdev->dev_speed == USBD_SPEED_HIGH ? HID_HS_BINTERVAL:HID_FS_BINTERVAL;
  (void)USBD_LL_OpenEP(pdev, HID_VIA_EP_IN, USBD_EP_TYPE_INTR, HID_VIA_EP_SIZE);
  pdev->ep_in[HID_VIA_EP_IN & 0xFU].is_used = 1U;

  pdev->ep_in[HID_VIA_EP_OUT & 0xFU].bInterval = pdev->dev_speed == USBD_SPEED_HIGH ? HID_HS_BINTERVAL:HID_FS_BINTERVAL;
  (void)USBD_LL_OpenEP(pdev, HID_VIA_EP_OUT, USBD_EP_TYPE_INTR, HID_VIA_EP_SIZE);
  pdev->ep_in[HID_VIA_EP_OUT & 0xFU].is_used = 1U;

  // EXK EP
  //
  pdev->ep_in[HID_EXK_EP_IN & 0xFU].bInterval = pdev->dev_speed == USBD_SPEED_HIGH ? HID_HS_BINTERVAL:HID_FS_BINTERVAL;
  (void)USBD_LL_OpenEP(pdev, HID_EXK_EP_IN, USBD_EP_TYPE_INTR, HID_EXK_EP_SIZE);
  pdev->ep_in[HID_EXK_EP_IN & 0xFU].is_used = 1U;


  hhid->state = USBD_HID_IDLE;

  /* Prepare Out endpoint to receive next packet */
  (void)USBD_LL_PrepareReceive(pdev, HID_VIA_EP_OUT, via_hid_usb_report, 32);


  static bool is_first = true;
  if (is_first)
  {
    is_first = false;

    qbufferCreateBySize(&report_q, (uint8_t *)report_buf, sizeof(report_info_t), 128); 
    qbufferCreateBySize(&via_report_q, (uint8_t *)via_report_q_buf, sizeof(via_report_info_t), 128); 
    qbufferCreateBySize(&report_exk_q, (uint8_t *)report_exk_buf, sizeof(report_info_t), 128); 

    logPrintf("[OK] USB Hid\n");
    logPrintf("     Keyboard\n");
    cliAdd("usbhid", cliCmd);

    usbHidInitTimer();
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_HID_DeInit
  *         DeInitialize the HID layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this class instance */
  HIDInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* Close HID EPs */
  (void)USBD_LL_CloseEP(pdev, HIDInEpAdd);
  pdev->ep_in[HIDInEpAdd & 0xFU].is_used = 0U;
  pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = 0U;

  /* Free allocated memory */
  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_HID_Setup
  *         Handle the HID specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t len;
  uint8_t *pbuf;
  uint16_t status_info = 0U;

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  logDebug("HID_SETUP %d\n", pdev->classId);
  logDebug("  req->bmRequest : 0x%X\n", req->bmRequest);
  logDebug("  req->bRequest  : 0x%X\n", req->bRequest);
  logDebug("       wIndex    : 0x%X\n", req->wIndex);
  logDebug("       wLength   : 0x%X %d\n", req->wLength, req->wLength);

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS :
      switch (req->bRequest)
      {
        case USBD_HID_REQ_SET_PROTOCOL:
          logDebug("  USBD_HID_REQ_SET_PROTOCOL  : 0x%X, 0x%d\n", req->wValue, req->wLength);      
          hhid->Protocol = (uint8_t)(req->wValue);
          break;

        case USBD_HID_REQ_GET_PROTOCOL:
          logDebug("  USBD_HID_REQ_GET_PROTOCOL  : 0x%X, 0x%d\n", req->wValue, req->wLength);      
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->Protocol, 1U);
          break;

        case USBD_HID_REQ_SET_IDLE:
          logDebug("  USBD_HID_REQ_SET_IDLE  : 0x%X, 0x%d\n", req->wValue, req->wLength);      
          hhid->IdleState = (uint8_t)(req->wValue >> 8);
          break;

        case USBD_HID_REQ_GET_IDLE:
          logDebug("  USBD_HID_REQ_GET_IDLE  : 0x%X, 0x%d\n", req->wValue, req->wLength);          
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->IdleState, 1U);
          break;

        case USBD_HID_REQ_SET_REPORT:  
          logDebug("  USBD_HID_REQ_SET_REPORT  : 0x%X, 0x%d\n", req->wValue, req->wLength);     
          ep0_req = *req;
          USBD_CtlPrepareRx(pdev, ep0_req_buf, req->wLength);
          break;

        default:
          logDebug("  ERROR  : 0x%X\n", req->wValue); 
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;
    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_DESCRIPTOR:
          logDebug("  USB_REQ_GET_DESCRIPTOR  : 0x%X\n", req->wValue); 
          if ((req->wValue >> 8) == HID_REPORT_DESC)
          {
            switch(req->wIndex)
            {
              case 1:
                len = MIN(HID_KEYBOARD_VIA_REPORT_DESC_SIZE, req->wLength);
                pbuf = HID_VIA_ReportDesc;
                break;

              case 2:
                len = MIN(HID_EXK_REPORT_DESC_SIZE, req->wLength);
                pbuf = HID_EXK_ReportDesc;
                break;

              default:
                len = MIN(HID_KEYBOARD_REPORT_DESC_SIZE, req->wLength);
                pbuf = HID_KEYBOARD_ReportDesc;
              break;
            }
          }
          else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE)
          {
            pbuf = USBD_HID_Desc;
            len = MIN(USB_HID_DESC_SIZ, req->wLength);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          (void)USBD_CtlSendData(pdev, pbuf, len);
          break;

        case USB_REQ_GET_INTERFACE :
          logDebug("  USB_REQ_GET_INTERFACE  : 0x%X\n", req->wValue); 
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->AltSetting, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          logDebug("  USB_REQ_SET_INTERFACE  : 0x%X\n", req->wValue); 
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hhid->AltSetting = (uint8_t)(req->wValue);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          logDebug("  USB_REQ_CLEAR_FEATURE  : 0x%X\n", req->wValue); 
          break;

        default:
          logDebug("  ERROR  : 0x%X\n", req->wValue); 
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

/**
  * @brief  USBD_HID_EP0_RxReady
  *         handle EP0 Rx Ready event
  * @param  pdev: device instance
  * @retval status
  */
uint8_t USBD_HID_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  logDebug("USBD_HID_EP0_RxReady()\n");
  logDebug("  req->bmRequest : 0x%X\n", ep0_req.bmRequest);
  logDebug("  req->bRequest  : 0x%X\n", ep0_req.bRequest);
  logDebug("  %d \n", ep0_req.wLength);
  for (int i=0; i<ep0_req.wLength; i++)
  {
    logDebug("  %d : 0x%02X\n", i, ep0_req_buf[i]);
  }

  if (ep0_req.bRequest == USBD_HID_REQ_SET_REPORT)
  {
    uint8_t led_bits = ep0_req_buf[0];

    usbHidSetStatusLed(led_bits);
  }
  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_HID_SendReport
  *         Send HID Report
  * @param  buff: pointer to report
  * @retval status
  */
bool USBD_HID_SendReport(uint8_t *report, uint16_t len)
{
  USBD_HandleTypeDef *pdev = &USBD_Device;
  bool ret = false;

  if (p_hhid == NULL)
  {
    return false;
  }

  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
    if (p_hhid->state == USBD_HID_IDLE)
    {
      ret = true;
      p_hhid->state = USBD_HID_BUSY;
      (void)USBD_LL_Transmit(pdev, HID_EPIN_ADDR, report, len);
    }
  }

  return ret;
}

/**
  * @brief  USBD_HID_SendReportEXK
  *         Send HID Report
  * @param  buff: pointer to report
  * @retval status
  */
bool USBD_HID_SendReportEXK(uint8_t *report, uint16_t len)
{
  USBD_HandleTypeDef *pdev = &USBD_Device;
  bool ret = false;

  if (p_hhid == NULL)
  {
    return false;
  }

  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
    if (p_hhid->state == USBD_HID_IDLE)
    {
      ret = true;
      p_hhid->state = USBD_HID_BUSY;
      (void)USBD_LL_Transmit(pdev, HID_EXK_EP_IN, report, len);
    }
  }

  return ret;
}

/**
  * @brief  USBD_HID_GetPollingInterval
  *         return polling interval from endpoint descriptor
  * @param  pdev: device instance
  * @retval polling interval
  */
uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev)
{
  uint32_t polling_interval;

  /* HIGH-speed endpoints */
  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* Sets the data transfer polling interval for high speed transfers.
     Values between 1..16 are allowed. Values correspond to interval
     of 2 ^ (bInterval-1). This option (8 ms, corresponds to HID_HS_BINTERVAL */
    polling_interval = (((1U << (HID_HS_BINTERVAL - 1U))) / 8U);
  }
  else   /* LOW and FULL-speed endpoints */
  {
    /* Sets the data transfer polling interval for low and full
    speed transfers */
    polling_interval =  HID_FS_BINTERVAL;
  }

  return ((uint32_t)(polling_interval));
}

#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
uint8_t *USBD_HID_GetUsrStrDescriptor(struct _USBD_HandleTypeDef *pdev, uint8_t index,  uint16_t *length)
{
  logPrintf("USBD_HID_GetUsrStrDescriptor() %d\n", index);
  return USBD_HID_ProductStrDescriptor(pdev->dev_speed, length);
}
#endif

#ifndef USE_USBD_COMPOSITE
/**
  * @brief  USBD_HID_GetCfgFSDesc
  *         return FS configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpDesc = USBD_GetEpDesc(USBD_HID_CfgDesc, HID_EPIN_ADDR);

  if (pEpDesc != NULL)
  {
    pEpDesc->bInterval = HID_FS_BINTERVAL;
  }

  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

/**
  * @brief  USBD_HID_GetCfgHSDesc
  *         return HS configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpDesc = USBD_GetEpDesc(USBD_HID_CfgDesc, HID_EPIN_ADDR);

  if (pEpDesc != NULL)
  {
    pEpDesc->bInterval = HID_HS_BINTERVAL;
  }

  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

/**
  * @brief  USBD_HID_GetOtherSpeedCfgDesc
  *         return other speed configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpDesc = USBD_GetEpDesc(USBD_HID_CfgDesc, HID_EPIN_ADDR);

  if (pEpDesc != NULL)
  {
    pEpDesc->bInterval = HID_FS_BINTERVAL;
  }

  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}
#endif /* USE_USBD_COMPOSITE  */

static uint32_t data_in_cnt = 0;
static uint32_t data_in_rate = 0;

static bool     rate_time_req = false;
static uint32_t rate_time_pre = 0;
static uint32_t rate_time_us  = 0;
static uint32_t rate_time_min = 0; 
static uint32_t rate_time_avg = 0; 
static uint32_t rate_time_sum = 0; 
static uint32_t rate_time_max = 0; 
static uint32_t rate_time_min_check = 0xFFFF; 
static uint32_t rate_time_max_check = 0; 

static uint32_t rate_time_sof_pre = 0; 
static uint32_t rate_time_sof = 0; 

static uint16_t rate_his_buf[100];

static bool     key_time_req = false;
static uint32_t key_time_pre;
static uint32_t key_time_end;
static uint32_t key_time_idx = 0;
static uint32_t key_time_cnt = 0;
static uint32_t key_time_log[KEY_TIME_LOG_MAX];
static bool     key_time_raw_req = false;
static uint32_t key_time_raw_pre;
static uint32_t key_time_raw_log[KEY_TIME_LOG_MAX];
static uint32_t key_time_pre_log[KEY_TIME_LOG_MAX];

/**
  * @brief  USBD_HID_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(epnum);
  /* Ensure that the FIFO is empty before a new transfer, this condition could
  be caused by  a new transfer before the end of the previous transfer */
  ((USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId])->state = USBD_HID_IDLE;

  if (epnum != (HID_EPIN_ADDR & 0x0F))
  {
    return (uint8_t)USBD_OK;
  }
  
  data_in_cnt++;


  usbHidMeasureRateTime();

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  /* Get the received data length */
  uint32_t rx_size;
  rx_size = USBD_LL_GetRxDataSize(pdev, epnum);

  if (via_hid_receive_func != NULL)
  {
    via_hid_receive_func(via_hid_usb_report, rx_size);
  }

  #if 0
  USBD_LL_Transmit(pdev, HID_VIA_EP_OUT, via_hid_usb_report, sizeof(via_hid_usb_report));
  USBD_LL_PrepareReceive(pdev, HID_VIA_EP_OUT, via_hid_usb_report, sizeof(via_hid_usb_report));
  #else
  via_report_info_t info;
  memcpy(info.buf, via_hid_usb_report, sizeof(via_hid_usb_report));
  qbufferWrite(&via_report_q, (uint8_t *)&info, 1);
  via_report_pre_time = millis();
  #endif
  return (uint8_t)USBD_OK;
}

uint8_t USBD_HID_SOF(USBD_HandleTypeDef *pdev)
{
  usbHidMeasurePollRate();

  if (qbufferAvailable(&via_report_q) && (millis()-via_report_pre_time) >= via_report_time)
  {
    qbufferRead(&via_report_q, (uint8_t *)via_hid_usb_report, 1);
    USBD_LL_Transmit(pdev, HID_VIA_EP_OUT, via_hid_usb_report, sizeof(via_hid_usb_report));
    USBD_LL_PrepareReceive(pdev, HID_VIA_EP_OUT, via_hid_usb_report, sizeof(via_hid_usb_report));
  }
  return (uint8_t)USBD_OK;
}

#ifndef USE_USBD_COMPOSITE
/**
  * @brief  DeviceQualifierDescriptor
  *         return Device Qualifier descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);

  return USBD_HID_DeviceQualifierDesc;
}
#endif /* USE_USBD_COMPOSITE  */


bool usbHidUpdateWakeUp(USBD_HandleTypeDef *pdev)
{
  PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;
  bool ret = false;
  
  if (pdev->dev_state == USBD_STATE_SUSPENDED)
  {
    logPrintf("[  ] USB WakeUp\n");

    __HAL_PCD_UNGATE_PHYCLOCK((hpcd));
    HAL_PCD_ActivateRemoteWakeup(hpcd);
    delay(10);
    HAL_PCD_DeActivateRemoteWakeup(hpcd);
    ret = true;
  }

  return ret;
}

bool usbHidSetViaReceiveFunc(void (*func)(uint8_t *, uint8_t))
{
  via_hid_receive_func = func;
  return true;
}

bool usbHidSendReport(uint8_t *p_data, uint16_t length)
{
  report_info_t report_info;

  if (length > HID_KEYBOARD_REPORT_SIZE)
    return false;

  if (!USBD_is_suspended())
  {
    key_time_pre = micros();

    memcpy(hid_buf, p_data, length);
    if (USBD_HID_SendReport((uint8_t *)hid_buf, HID_KEYBOARD_REPORT_SIZE))
    {
      key_time_req = true;
      rate_time_req = true;
      rate_time_pre = micros();    
    }  
    else
    {
      memcpy(report_info.buf, p_data, length);
      qbufferWrite(&report_q, (uint8_t *)&report_info, 1);        
    }    
  }
  else
  {
    usbHidUpdateWakeUp(&USBD_Device);
  }
  
  return true;
}

bool usbHidSendReportEXK(uint8_t *p_data, uint16_t length)
{
  exk_report_info_t report_info;

  if (length > HID_EXK_EP_SIZE)
    return false;

  if (!USBD_is_suspended())
  {
    memcpy(hid_buf_exk, p_data, length);
    if (!USBD_HID_SendReportEXK((uint8_t *)hid_buf_exk, length))
    {
      report_info.len = length;
      memcpy(report_info.buf, p_data, length);
      qbufferWrite(&report_exk_q, (uint8_t *)&report_info, 1);        
    }    
  }
  else
  {
    usbHidUpdateWakeUp(&USBD_Device);
  }
  
  return true;
}

void usbHidMeasurePollRate(void)
{
  static uint32_t cnt = 0; 


  rate_time_sof_pre = micros();
  if (cnt >= 8000)
  {
    cnt = 0;
    data_in_rate = data_in_cnt;
    rate_time_min = rate_time_min_check; 
    rate_time_max = rate_time_max_check;     
    rate_time_avg = rate_time_sum / (data_in_cnt + 1);
    data_in_cnt = 0;

    rate_time_min_check = 0xFFFF; 
    rate_time_max_check = 0;     
    rate_time_sum = 0;
  }  
  cnt++;  
}

void usbHidMeasureRateTime(void)
{
  rate_time_sof = micros() - rate_time_sof_pre;

  if (rate_time_req)
  {
    uint32_t rate_time_cur;
    
    rate_time_cur = micros();
    rate_time_us  = rate_time_cur - rate_time_pre;
    rate_time_sum += rate_time_us; 
    if (rate_time_min_check > rate_time_us)
    {
      rate_time_min_check = rate_time_us;
    }
    if (rate_time_max_check < rate_time_us)
    {
      rate_time_max_check = rate_time_us;
    }


    uint32_t rate_time_idx;

    rate_time_idx = constrain(rate_time_us/10, 0, 99);
    if (rate_his_buf[rate_time_idx] < 0xFFFF)
    {
      rate_his_buf[rate_time_idx]++;
    }  

    rate_time_req = false;
  }

  if (key_time_req)
  {
    key_time_end = micros()-key_time_pre;
    key_time_req = false;

    key_time_log[key_time_idx] = key_time_end;

    if (key_time_raw_req)
    {
      key_time_raw_req = false;
      key_time_raw_log[key_time_idx] = micros()-key_time_raw_pre;
      key_time_pre_log[key_time_idx] = key_time_pre-key_time_raw_pre;
    }
    else
    {
      key_time_raw_log[key_time_idx] = key_time_end;
    }

    key_time_idx = (key_time_idx + 1) % KEY_TIME_LOG_MAX;
    if (key_time_cnt < KEY_TIME_LOG_MAX)
    {
      key_time_cnt++;
    }
  }  
}

bool usbHidGetRateInfo(usb_hid_rate_info_t *p_info)
{
  p_info->freq_hz = data_in_rate;
  p_info->time_max = rate_time_max;
  p_info->time_min = rate_time_min;
  return true;
}

bool usbHidSetTimeLog(uint16_t index, uint32_t time_us)
{
  key_time_raw_pre = time_us;
  key_time_raw_req = true;
  return true;
}

__weak void usbHidSetStatusLed(uint8_t led_bits)
{

}

void usbHidInitTimer(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 159;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_COMBINED_RESETTRIGGER;
  sSlaveConfig.InputTrigger = TIM_TS_ITR11;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 120;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM2)
  {
    /* TIM2 clock enable */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* TIM2 interrupt Init */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM2)
  {
    /* Peripheral clock disable */
    __HAL_RCC_TIM2_CLK_DISABLE();

    /* TIM2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
  }
}

void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim2);
}

volatile int timer_cnt = 0;
volatile uint32_t timer_end = 0;

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  timer_cnt++;
  timer_end = micros()-rate_time_sof_pre;


  if (qbufferAvailable(&report_q) > 0)
  {
    if (p_hhid->state == USBD_HID_IDLE)
    {
      qbufferRead(&report_q, (uint8_t *)hid_buf, 1);
      key_time_req = true;

      USBD_HID_SendReport((uint8_t *)hid_buf, HID_KEYBOARD_REPORT_SIZE);
      rate_time_req = true;
      rate_time_pre = micros();
    }
  }

  if (qbufferAvailable(&report_exk_q) > 0)
  {
    if (p_hhid->state == USBD_HID_IDLE)
    {
      exk_report_info_t report_info;

      qbufferRead(&report_exk_q, (uint8_t *)&report_info, 1);

      memcpy(hid_buf_exk, report_info.buf, report_info.len);
      USBD_HID_SendReportEXK((uint8_t *)hid_buf_exk, report_info.len);
    }
  }

  return;
}

#ifdef _USE_HW_CLI
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    ret = true;
  }

  if (args->argc >= 1 && args->isStr(0, "rate") == true)
  {
    uint32_t pre_time;
    uint32_t pre_time_key;
    uint32_t key_send_cnt = 0;

    memset(rate_his_buf, 0, sizeof(rate_his_buf));

    pre_time = millis();
    pre_time_key = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time_key >= 2 && key_send_cnt < 50)
      {
        uint8_t buf[HID_KEYBOARD_REPORT_SIZE];

        memset(buf, 0, HID_KEYBOARD_REPORT_SIZE);

        pre_time_key = millis();    
        usbHidSendReport(buf, HID_KEYBOARD_REPORT_SIZE);      
        key_send_cnt++;
      }

      
      if (millis()-pre_time >= 1000)
      {
        pre_time = millis();
        cliPrintf("hid rate %d Hz, avg %4d us, max %4d us, min %d us, %d, %d\n", 
          data_in_rate,
          rate_time_avg,
          rate_time_max,
          rate_time_min,
          rate_time_sof,
          timer_end
          ); 
        
        for (int i=0; i<10; i++)
        {
          cliPrintf("%d us\n",key_time_log[i]);
        }
        timer_cnt = 0;
        key_send_cnt = 0;
      }
    }

    if (args->argc == 2 && args->isStr(1, "his"))
    {
      for (int i=0; i<100; i++)
      {
        cliPrintf("%d %d\n", i, rate_his_buf[i]);
      }
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "log") && args->isStr(1, "clear"))
  {
    key_time_idx = 0;
    key_time_cnt = 0;
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "log") == true)
  {
    uint16_t index;
    uint16_t time_max[3] = {0, 0, 0};
    uint16_t time_min[3] = {0xFFFF, 0xFFFF, 0xFFFF};
    uint16_t time_sum[3] = {0, 0, 0};


    for (int i = 0; i < key_time_cnt; i++)
    {
      if (key_time_cnt == KEY_TIME_LOG_MAX)
        index = (key_time_idx + i) % KEY_TIME_LOG_MAX;
      else
        index = i;

      cliPrintf("%2d: %3d us, raw : %3d us, %d\n",
                i,
                key_time_log[index],
                key_time_raw_log[index],
                key_time_pre_log[index]);

      for (int j=0; j<3; j++)
      {
        uint16_t data;

        if (j == 0)
          data = key_time_log[index]; 
        else if (j == 1)
          data = key_time_raw_log[index]; 
        else
          data = key_time_pre_log[index];          

        time_sum[j] += data;
        if (data > time_max[j])
          time_max[j] = data;
        if (data < time_min[j])
          time_min[j] = data;
      }
    }

    cliPrintf("\n");
    if (key_time_cnt > 0)
    {
      cliPrintf("avg : %3d us %3d us %3d us\n",
                time_sum[0] / key_time_cnt,
                time_sum[1] / key_time_cnt,
                time_sum[2] / key_time_cnt);
      cliPrintf("max : %3d us %3d us %3d us\n", time_max[0], time_max[1], time_max[2]);
      cliPrintf("min : %3d us %3d us %3d us\n", time_min[0], time_min[1], time_min[2]);
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usbhid info\n");
    cliPrintf("usbhid rate\n");
    cliPrintf("usbhid rate his\n");
    cliPrintf("usbhid log\n");
    cliPrintf("usbhid log clear\n");
  }
}
#endif
