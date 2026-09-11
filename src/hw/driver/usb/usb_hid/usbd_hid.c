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
#define HID_KEYBOARD_BOOT_SIZE   (HW_KEYS_BOOT_MAX + 2U)

// 리포트가 EP 를 넘으면 조용히 안 나간다. 키 수를 손댈 때 빌드에서 잡는다.
_Static_assert(HID_KEYBOARD_BOOT_SIZE <= HID_EPIN_SIZE,
               "부트 리포트가 IF0 엔드포인트보다 크다");
_Static_assert((1U + HID_KEYBOARD_REPORT_SIZE) <= HID_EXK_EP_SIZE,
               "확장 키 리포트가 IF2 엔드포인트보다 크다");
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
static bool usbHidTransmitKbd(bool is_boot, const uint8_t *state);
static bool usbHidKbdEpBusy(void);
static void usbHidUpdateRoute(void);
static bool usbHidWantBootRoute(void);





typedef struct
{
  uint8_t  buf[32];
} via_report_info_t;

typedef struct
{
  uint8_t len;
  uint8_t buf[HID_EXK_EP_SIZE];
} exk_report_info_t;

// 측정 시각을 리포트마다 들고 다닌다. 전역 스칼라로 두면 큐에 리포트가 밀렸을 때
// DataIn 이 완료된 리포트를 더 나중 스캔의 시각과 짝지어 언더플로우한다.
typedef struct
{
  uint8_t  buf[HID_KEYBOARD_REPORT_SIZE];
  uint32_t time_pre;
  uint32_t raw_pre;
  uint32_t proc_pre;
  uint32_t qmk_cyc;
  bool     raw_req;
  bool     proc_req;
  bool     qmk_req;
} kbd_report_info_t;

static USBD_SetupReqTypedef ep0_req;
static uint8_t ep0_req_buf[USB_MAX_EP0_SIZE];

static qbuffer_t             via_report_q;
static via_report_info_t     via_report_q_buf[128];
static uint32_t              via_report_pre_time;
static uint32_t              via_report_time = 20;
__ALIGN_BEGIN static uint8_t via_hid_usb_report[32] __ALIGN_END;
static void (*via_hid_receive_func)(uint8_t *data, uint8_t length) = NULL;


__ALIGN_BEGIN  static uint8_t hid_buf[HID_KEYBOARD_REPORT_SIZE] __ALIGN_END = {0,};
static qbuffer_t              report_kbd_q;
static kbd_report_info_t      report_kbd_buf[128];
static volatile bool          kbd_ep_busy    = false;

static qbuffer_t              report_exk_q;
static exk_report_info_t      report_exk_buf[128];
__ALIGN_BEGIN  static uint8_t hid_buf_exk[HID_EXK_EP_SIZE] __ALIGN_END = {0,};
static volatile bool          exk_ep_busy    = false;

// 확장 경로의 키 리포트는 EXK 와 같은 EP 로 나간다. 전송 버퍼를 나눠 두면 어느 쪽이
// 실려 있든 DMA 가 읽는 메모리가 섞이지 않는다.
__ALIGN_BEGIN  static uint8_t hid_buf_extk[1 + HID_KEYBOARD_REPORT_SIZE] __ALIGN_END = {0,};

/*
 * ── 부트 프로토콜 경로 ──────────────────────────────────────────────────
 *
 * IF0 은 부트 서브클래스라 8바이트(mods, reserved, keys[6]) 만 낼 수 있다.
 * 20키 롤오버는 비부트인 IF2 의 확장 컬렉션으로 나간다. 둘을 동시에 내면 호스트가
 * 같은 키를 두 번 받으므로 한 번에 한쪽만 쓴다.
 *
 * 어느 쪽을 쓸지는 두 신호로 정한다.
 *   kbd_protocol   : 호스트가 SET_PROTOCOL 로 정한다. 0=부트, 1=리포트 (기본 1)
 *   extk_desc_read : IF2 의 리포트 기술자를 요청받았다 = OS 급 호스트가 열거했다
 *
 * ★ SET_PROTOCOL 만 보면 모자란다. 부트를 가정하고 SET_PROTOCOL 을 아예 안 보내는
 *   BIOS 가 있고, 그런 호스트에서는 IF0 이 조용해진다. 기술자 요청까지 함께 봐야
 *   그 경우도 IF0 을 받는다 - BIOS 는 비부트 인터페이스의 기술자를 읽지 않는다.
 */
static volatile uint8_t       kbd_protocol       = 1;
static volatile bool          extk_desc_read     = false;
static volatile bool          route_is_boot      = true;   // 열거 전에는 부트가 안전하다
static volatile bool          extk_kbd_in_flight = false;  // EXK EP 에 실린 것이 키 리포트인가

// 마지막으로 실은 키 상태(22바이트). GET_REPORT 응답과 경로 전환에 쓴다.
static uint8_t                kbd_state_last[HID_KEYBOARD_REPORT_SIZE] = {0,};

// EP0 응답 버퍼. USBD_CtlSendData 는 포인터만 들고 가므로 스택을 넘기면 안 된다.
__ALIGN_BEGIN  static uint8_t ep0_rep_buf[1 + HID_KEYBOARD_REPORT_SIZE] __ALIGN_END = {0,};
static uint8_t                ep0_protocol_buf = 1;



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

/*
 * IF0 - 부트 키보드. HID 1.11 Appendix B 가 정한 8바이트 형식 그대로다.
 *
 * BIOS/UEFI 는 이 기술자를 읽지 않는다. SET_PROTOCOL(0) 을 걸고 고정 8바이트
 * (mods, reserved, keys[6]) 를 읽을 뿐이다. 그래서 여기에 20키를 넣으면 규격을
 * 벗어나고, 8바이트만 받을 준비를 한 호스트에서는 키가 아예 안 먹는다.
 * 20키 롤오버는 IF2 의 확장 컬렉션이 담당한다.
 */
__ALIGN_BEGIN static uint8_t HID_KEYBOARD_ReportDesc[] __ALIGN_END =
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
  0x95, HW_KEYS_BOOT_MAX,             //   REPORT_COUNT (6)
  0x75, 0x08,                         //   REPORT_SIZE (8)
  0x15, 0x00,                         //   LOGICAL_MINIMUM (0)
  0x26, 0xFF, 0x00,                   //   LOGICAL_MAXIMUM (255)
  0x05, 0x07,                         //   USAGE_PAGE (Keyboard)
  0x19, 0x00,                         //   USAGE_MINIMUM (Reserved (no event indicated))
  0x29, 0xFF,                         //   USAGE_MAXIMUM (Keyboard Application)
  0x81, 0x00,                         //   INPUT (Data,Ary,Abs)
  0xc0                                // END_COLLECTION
};

// 크기가 어긋나면 열거가 조용히 깨진다. 빌드에서 잡는다.
_Static_assert(sizeof(HID_KEYBOARD_ReportDesc) == HID_KEYBOARD_REPORT_DESC_SIZE,
               "HID_KEYBOARD_REPORT_DESC_SIZE 가 기술자 실제 크기와 다르다");

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

/*
 * IF2 - 비부트 인터페이스. 시스템/컨슈머/마우스에 확장 키보드를 더한다.
 *
 * 20키 롤오버는 여기로 나간다. IF0 은 규격대로 8바이트만 내야 하고, 둘을 동시에
 * 내면 호스트가 키를 두 번 받으므로 한 번에 한쪽만 쓴다 (usbHidIsBootRoute).
 *
 * LED 출력은 일부러 넣지 않았다. IF0 에 이미 있고, 두 곳에 두면 SET_REPORT 가
 * 리포트 ID 를 데이터에 포함하는지가 호스트마다 갈려 LED 처리가 흔들린다.
 */
__ALIGN_BEGIN static uint8_t HID_EXK_ReportDesc[] __ALIGN_END =
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
  0xC0,                     // End Collection

  /* ======= 마우스 기능 ======= */
  0x05, 0x01,               // Usage Page (Generic Desktop Ctrls)
  0x09, 0x02,               // Usage (Mouse)
  0xA1, 0x01,               // Collection (Application)
  0x85, REPORT_ID_MOUSE,    //   Report ID
  0x09, 0x01,               //   Usage (Pointer)
  0xA1, 0x00,               //   Collection (Physical)
  0x05, 0x09,               //     Usage Page (Button)
  0x19, 0x01,               //     Usage Minimum (0x01)
  0x29, 0x05,               //     Usage Maximum (0x05)
  0x15, 0x00,               //     Logical Minimum (0)
  0x25, 0x01,               //     Logical Maximum (1)
  0x95, 0x05,               //     Report Count (5)
  0x75, 0x01,               //     Report Size (1)
  0x81, 0x02,               //     Input (Data,Var,Abs)
  0x95, 0x01,               //     Report Count (1)
  0x75, 0x03,               //     Report Size (3) 패딩
  0x81, 0x01,               //     Input (Const,Array,Abs)
  0x05, 0x01,               //     Usage Page (Generic Desktop Ctrls)
  0x09, 0x30,               //     Usage (X)
  0x09, 0x31,               //     Usage (Y)
  0x09, 0x38,               //     Usage (Wheel)
  0x15, 0x81,               //     Logical Minimum (-127)
  0x25, 0x7F,               //     Logical Maximum (127)
  0x75, 0x08,               //     Report Size (8)
  0x95, 0x03,               //     Report Count (3)
  0x81, 0x06,               //     Input (Data,Var,Rel)
  0xC0,                     //   End Collection
  0xC0,                     // End Collection

  /* ======= 확장 키보드 (20키) ======= */
  0x05, 0x01,                 // Usage Page (Generic Desktop)
  0x09, 0x06,                 // Usage (Keyboard)
  0xA1, 0x01,                 // Collection (Application)
  0x85, REPORT_ID_KEYBOARD,   //   Report ID
  0x05, 0x07,                 //   Usage Page (Keyboard)
  0x19, 0xE0,                 //   Usage Minimum (Keyboard LeftControl)
  0x29, 0xE7,                 //   Usage Maximum (Keyboard Right GUI)
  0x15, 0x00,                 //   Logical Minimum (0)
  0x25, 0x01,                 //   Logical Maximum (1)
  0x75, 0x01,                 //   Report Size (1)
  0x95, 0x08,                 //   Report Count (8)
  0x81, 0x02,                 //   Input (Data,Var,Abs)   모디파이어
  0x95, 0x01,                 //   Report Count (1)
  0x75, 0x08,                 //   Report Size (8)
  0x81, 0x03,                 //   Input (Cnst,Var,Abs)   예약
  0x95, HW_KEYS_PRESS_MAX,    //   Report Count (20)
  0x75, 0x08,                 //   Report Size (8)
  0x15, 0x00,                 //   Logical Minimum (0)
  0x26, 0xFF, 0x00,           //   Logical Maximum (255)
  0x05, 0x07,                 //   Usage Page (Keyboard)
  0x19, 0x00,                 //   Usage Minimum (Reserved)
  0x29, 0xFF,                 //   Usage Maximum (Keyboard Application)
  0x81, 0x00,                 //   Input (Data,Ary,Abs)
  0xC0                        // End Collection
};

_Static_assert(sizeof(HID_EXK_ReportDesc) == HID_EXK_REPORT_DESC_SIZE,
               "HID_EXK_REPORT_DESC_SIZE 가 기술자 실제 크기와 다르다");

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

  // 재열거 도중 전송이 끊기면 busy 가 참으로 굳고, 완료 인터럽트가 영영 안 오므로
  // 그 EP 가 조용히 죽는다. 설정이 새로 잡히는 이 자리에서 되돌린다.
  kbd_ep_busy        = false;
  exk_ep_busy        = false;
  extk_kbd_in_flight = false;

  /* Prepare Out endpoint to receive next packet */
  (void)USBD_LL_PrepareReceive(pdev, HID_VIA_EP_OUT, via_hid_usb_report, 32);


  static bool is_first = true;
  if (is_first)
  {
    is_first = false;

    qbufferCreateBySize(&via_report_q, (uint8_t *)via_report_q_buf, sizeof(via_report_info_t), 128);
    qbufferCreateBySize(&report_exk_q, (uint8_t *)report_exk_buf, sizeof(exk_report_info_t), 128);
    qbufferCreateBySize(&report_kbd_q, (uint8_t *)report_kbd_buf, sizeof(kbd_report_info_t), 128);

    logPrintf("[OK] USB Hid\n");
    logPrintf("     Keyboard\n");
    cliAdd("usbhid", cliCmd);

#if _USE_HW_USB_EOPF == 0
    usbHidInitTimer();
#endif
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
          // 모든 인터페이스가 이리로 온다. 부트 서브클래스를 가진 것은 IF0 뿐이라
          // 그것만 본다. 여기서는 값만 기록한다 - 눌린 키를 비우는 것은 QMK 의 일이고
          // 여기는 제어 전송 안이다 (qmkUpdate 가 경로 변화를 보고 한다).
          if (req->wIndex == 0U)
          {
            kbd_protocol = (req->wValue == 0U) ? 0U : 1U;
          }
          break;

        case USBD_HID_REQ_GET_PROTOCOL:
          logDebug("  USBD_HID_REQ_GET_PROTOCOL  : 0x%X, 0x%d\n", req->wValue, req->wLength);      
          ep0_protocol_buf = (req->wIndex == 0U) ? kbd_protocol : 1U;
          (void)USBD_CtlSendData(pdev, &ep0_protocol_buf, 1U);
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

        case USBD_HID_REQ_GET_REPORT:
          // 초기화 때 GET_REPORT 를 던지는 호스트가 있다. 처리하지 않으면 STALL 이
          // 나가고, 그걸 실패로 보는 드라이버는 키보드를 아예 안 붙인다.
          logDebug("  USBD_HID_REQ_GET_REPORT  : 0x%X, 0x%d\n", req->wValue, req->wLength);
          if (req->wIndex == 0U)
          {
            memset(ep0_rep_buf, 0, HID_KEYBOARD_BOOT_SIZE);
            ep0_rep_buf[0] = kbd_state_last[0];
            memcpy(&ep0_rep_buf[2], &kbd_state_last[2], HW_KEYS_BOOT_MAX);
            len = MIN(HID_KEYBOARD_BOOT_SIZE, req->wLength);
          }
          else
          {
            ep0_rep_buf[0] = REPORT_ID_KEYBOARD;
            memcpy(&ep0_rep_buf[1], kbd_state_last, HID_KEYBOARD_REPORT_SIZE);
            len = MIN((uint16_t)sizeof(ep0_rep_buf), req->wLength);
          }
          (void)USBD_CtlSendData(pdev, ep0_rep_buf, len);
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
                // BIOS 는 비부트 인터페이스의 기술자를 읽지 않는다. 이 요청이 왔다는
                // 것은 OS 급 호스트가 IF2 를 열거했다는 뜻이다 -> 확장 경로를 쓴다.
                extk_desc_read = true;
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
    if (!kbd_ep_busy)
    {
      ret = true;
      kbd_ep_busy = true;
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
    if (!exk_ep_busy)
    {
      ret = true;
      exk_ep_busy = true;
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

// USB 링크(케이블) 상태 간접 지표 (USB ISR/콜백에서만 갱신)
static uint32_t link_reset_count   = 0;
static uint32_t link_suspend_count = 0;

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
static uint32_t key_time_seq = 0;   // 실제 매트릭스 키 입력 측정마다 증가 (웹 신규 샘플 감지)
static uint16_t key_time_last_raw = 0;  // 누름 -> USB 전송 (디바운스 포함 총 지연)
static uint16_t key_time_last_pre = 0;  // 누름 -> 리포트 큐잉 (펌웨어 처리)
static uint16_t key_time_last_usb = 0;  // 큐잉 -> USB 전송 (USB 구간)

static bool     key_time_proc_req = false;
static uint32_t key_time_proc_pre;      // 디바운스 확정 스캔 시작 시각
static uint32_t key_time_proc_log[KEY_TIME_LOG_MAX];  // 확정스캔 -> 큐잉 (디바운스 제외 CPU)
static uint16_t key_time_last_proc = 0;

// proc CPU 세분: scan(캡처read)/decode(전치)/qmk(matrix_scan종료→큐잉). DWT 사이클.
static bool     key_break_req = false;
static uint32_t key_break_anchor = 0;   // matrix_scan 종료 시점 DWT cyc
static uint32_t key_break_scan_cyc = 0;
static uint32_t key_break_decode_cyc = 0;
static uint32_t key_break_qmk_cyc = 0;  // 최근 keystroke qmk 사이클

/**
  * @brief  USBD_HID_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(pdev);

  if (epnum == (HID_EXK_EP_IN & 0x0F))
  {
    exk_ep_busy = false;

    // 확장 경로에서는 키 리포트도 이 EP 로 나간다. 레이턴시 측정은 키 리포트만 따른다.
    if (extk_kbd_in_flight)
    {
      extk_kbd_in_flight = false;
      data_in_cnt++;
      usbHidMeasureRateTime();
    }

    usbHidFlush();
    return (uint8_t)USBD_OK;
  }

  if (epnum != (HID_EPIN_ADDR & 0x0F))
  {
    return (uint8_t)USBD_OK;
  }

  kbd_ep_busy = false;
  data_in_cnt++;

  usbHidMeasureRateTime();
  usbHidFlush();

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

// 리포트는 큐에만 넣는다. 여기서 직접 전송하면 큐에 밀린 리포트를 추월해
// 순서가 뒤집히고, 절대상태인 HID 리포트는 오래된 상태로 굳어버린다.
bool usbHidSendReport(uint8_t *p_data, uint16_t length)
{
  kbd_report_info_t report_info;

  if (length > HID_KEYBOARD_REPORT_SIZE)
    return false;

  if (!USBD_is_suspended())
  {
    memset(report_info.buf, 0, sizeof(report_info.buf));
    memcpy(report_info.buf, p_data, length);
    report_info.time_pre = micros();
    report_info.qmk_cyc  = DWT->CYCCNT - key_break_anchor;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    report_info.qmk_req  = key_break_req;   // qmk = matrix_scan 종료 -> 리포트 큐잉
    report_info.raw_req  = key_time_raw_req;
    report_info.raw_pre  = key_time_raw_pre;
    report_info.proc_req = key_time_proc_req;
    report_info.proc_pre = key_time_proc_pre;
    key_break_req     = false;
    key_time_raw_req  = false;
    key_time_proc_req = false;

    if (!qbufferWrite(&report_kbd_q, (uint8_t *)&report_info, 1))
    {
      // 큐가 차면 가장 오래된 것을 버린다. 최신 리포트를 버리면 호스트가 낡은
      // 상태(눌림)로 굳어 자동 반복이 걸리고 영영 복구되지 않는다.
      kbd_report_info_t drop;

      qbufferRead(&report_kbd_q, (uint8_t *)&drop, 1);
      qbufferWrite(&report_kbd_q, (uint8_t *)&report_info, 1);
    }
    __set_PRIMASK(primask);

    usbHidFlush();
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
    report_info.len = length;
    memcpy(report_info.buf, p_data, length);
    qbufferWrite(&report_exk_q, (uint8_t *)&report_info, 1);
  }
  else
  {
    usbHidUpdateWakeUp(&USBD_Device);
  }

  return true;
}

bool usbHidSendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
  uint8_t buffer[5];

  buffer[0] = REPORT_ID_MOUSE;   // enum 에 의해 자동으로 2 가 대입됨
  buffer[1] = buttons;
  buffer[2] = (uint8_t)x;
  buffer[3] = (uint8_t)y;
  buffer[4] = (uint8_t)wheel;

  return usbHidSendReportEXK(buffer, 5);
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
      uint32_t raw_us = micros()-key_time_raw_pre;
      uint32_t pre_us = key_time_pre-key_time_raw_pre;

      key_time_raw_req = false;
      key_time_raw_log[key_time_idx] = raw_us;
      key_time_pre_log[key_time_idx] = pre_us;

      // 실제 매트릭스 키 입력으로 발생한 리포트만 유효 샘플로 노출한다.
      // (연결 시 초기 리포트 등 매트릭스와 무관한 전송은 제외 -> 잡음 0.05ms 방지)
      key_time_last_raw = (raw_us > 0xFFFF) ? 0xFFFF : raw_us;
      key_time_last_pre = (pre_us > 0xFFFF) ? 0xFFFF : pre_us;
      key_time_last_usb = (key_time_end > 0xFFFF) ? 0xFFFF : key_time_end;
      key_time_seq++;
    }
    else
    {
      key_time_raw_log[key_time_idx] = key_time_end;
    }

    // proc = 확정스캔 시작 -> 큐잉 (디바운스 제외 순수 CPU: 스캔+디코드+QMK)
    if (key_time_proc_req)
    {
      uint32_t proc_us = key_time_pre - key_time_proc_pre;
      key_time_proc_req = false;
      key_time_proc_log[key_time_idx] = proc_us;
      key_time_last_proc = (proc_us > 0xFFFF) ? 0xFFFF : proc_us;
    }
    else
    {
      key_time_proc_log[key_time_idx] = 0;
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

void usbHidGetLinkHealth(usb_link_health_t *p_info)
{
  p_info->reset_count     = link_reset_count;
  p_info->suspend_count   = link_suspend_count;
  p_info->sof_stall_count = usbLinkGetSofStall();
  p_info->sof_rate        = usbLinkGetSofRate();
  p_info->uptime_s        = millis() / 1000;
}

void usbHidResetLinkHealth(void)
{
  link_reset_count   = 0;
  link_suspend_count = 0;
  usbLinkResetSofStall();
}

void usbHidLinkOnReset(void)
{
  link_reset_count++;

  // ★ 재열거에서 되돌린다. 그러지 않으면 BIOS 를 거쳐 부팅한 뒤 OS 에서도 부트
  //   프로토콜로 남아 20키가 안 나간다.
  kbd_protocol   = 1;
  extk_desc_read = false;
  route_is_boot  = true;
}

void usbHidLinkOnSuspend(void)
{
  link_suspend_count++;
}

// 가장 최근에 측정된 키 입력 레이턴시(us)를 반환한다.
//   raw_us : 스캔 확정 -> USB 실제 전송 (디바운스 제외 총 지연)
//   pre_us : 스캔 확정 -> 리포트 큐잉 (펌웨어 처리)
//   usb_us : 리포트 큐잉 -> USB 실제 전송 (USB 구간)
//   seq    : 새 측정마다 증가하는 카운터 (신규 샘플 감지용)
bool usbHidGetLatency(uint16_t *raw_us, uint16_t *pre_us, uint16_t *usb_us, uint32_t *seq)
{
  if (raw_us) *raw_us = key_time_last_raw;
  if (pre_us) *pre_us = key_time_last_pre;
  if (usb_us) *usb_us = key_time_last_usb;
  if (seq)    *seq    = key_time_seq;
  return (key_time_seq > 0);
}

bool usbHidSetTimeLog(uint16_t index, uint32_t time_us)
{
  key_time_raw_pre = time_us;
  key_time_raw_req = true;
  return true;
}

// 디바운스 확정 스캔 시작 시각 등록. proc = (큐잉 - 이 시각) = 디바운스 제외 CPU 처리시간.
bool usbHidSetProcTime(uint32_t time_us)
{
  key_time_proc_pre = time_us;
  key_time_proc_req = true;
  return true;
}

// proc 세분용: 이번 스캔의 scan/decode 사이클 + qmk 측정 앵커(matrix_scan 종료 DWT cyc).
bool usbHidSetProcBreak(uint32_t scan_cyc, uint32_t decode_cyc, uint32_t anchor_cyc)
{
  key_break_scan_cyc   = scan_cyc;
  key_break_decode_cyc = decode_cyc;
  key_break_anchor     = anchor_cyc;
  key_break_req        = true;
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

uint8_t usbHidGetProtocol(void)
{
  return kbd_protocol;
}

bool usbHidIsBootRoute(void)
{
  return route_is_boot;
}

/*
 * 시험용이다. 평소에는 호스트가 정한다.
 *
 * 부트 프로토콜을 요구하는 것은 BIOS·부트로더뿐이라 책상에서는 재현할 방법이 없다.
 * 같은 자리에 값을 넣어 흉내 낸다 - 그러지 않으면 "BIOS 에서 키가 먹나" 를 영영
 * 시험할 수 없다.
 */
void usbHidSetProtocolTest(uint8_t protocol)
{
  kbd_protocol = (protocol == 0) ? 0 : 1;
}

// 지금 신호로는 어느 경로를 써야 하는가. route_is_boot 는 전환이 끝난 뒤에 따라온다.
static bool usbHidWantBootRoute(void)
{
  return (kbd_protocol == 0) || (extk_desc_read == false);
}

// 지금 경로의 EP 가 바쁜가. 두 경로는 서로 다른 EP 를 쓴다.
static bool usbHidKbdEpBusy(void)
{
  return route_is_boot ? kbd_ep_busy : exk_ep_busy;
}

// state 가 NULL 이면 0 리포트(전부 뗌)를 낸다. 실었으면 true.
static bool usbHidTransmitKbd(bool is_boot, const uint8_t *state)
{
  // 전송 중인 DMA 버퍼를 덮어쓰면 선에 실리는 내용이 섞인다. 손대기 전에 막는다.
  if (is_boot ? kbd_ep_busy : exk_ep_busy)
    return false;

  if (state != NULL)
  {
    memcpy(kbd_state_last, state, HID_KEYBOARD_REPORT_SIZE);
  }
  else
  {
    memset(kbd_state_last, 0, HID_KEYBOARD_REPORT_SIZE);
  }

  if (is_boot)
  {
    // mods, reserved, keys[0..5] 만 남긴다. 부트 프로토콜은 8바이트가 규격이다.
    hid_buf[0] = kbd_state_last[0];
    hid_buf[1] = 0;
    memcpy(&hid_buf[2], &kbd_state_last[2], HW_KEYS_BOOT_MAX);

    return USBD_HID_SendReport((uint8_t *)hid_buf, HID_KEYBOARD_BOOT_SIZE);
  }

  hid_buf_extk[0] = REPORT_ID_KEYBOARD;
  memcpy(&hid_buf_extk[1], kbd_state_last, HID_KEYBOARD_REPORT_SIZE);

  extk_kbd_in_flight = true;
  if (USBD_HID_SendReportEXK((uint8_t *)hid_buf_extk, sizeof(hid_buf_extk)))
  {
    return true;
  }

  extk_kbd_in_flight = false;
  return false;
}

/*
 * 경로가 갈리면 옛 인터페이스에 눌린 키가 그대로 남는다 - 아무도 안 뗀다.
 * 0 리포트를 한 번 내보내고 나서 경로를 넘긴다. 못 실으면 다음 호출에 다시 온다.
 *
 * QMK 내부 상태를 비우는 것은 여기가 아니라 qmkUpdate 가 한다. 이 함수는 ISR 에서도
 * 불리므로 상위 층 함수를 부르면 층이 뒤집힌다.
 */
static void usbHidUpdateRoute(void)
{
  bool want_boot = usbHidWantBootRoute();

  if (want_boot == route_is_boot)
    return;

  if (usbHidTransmitKbd(route_is_boot, NULL) == false)
    return;

  route_is_boot = want_boot;
  qbufferFlush(&report_kbd_q);   // 옛 경로로 쌓인 것은 이미 낡았다
}

// 메인 루프 / DataIn / TIM2 ISR 에서 모두 호출된다. ep_busy 가드 안에서
// 가장 오래된 것 하나만 꺼내므로 몇 번을 호출하든 멱등이고 순서도 보존된다.
// TIM2(SOF 종속) 가 멈춰도 나머지 두 경로가 큐를 비운다.
void usbHidFlush(void)
{
  bool route_req = (usbHidWantBootRoute() != route_is_boot);

  if (route_req == false &&
      qbufferAvailable(&report_kbd_q) == 0 && qbufferAvailable(&report_exk_q) == 0)
  {
    return;   // 메인 루프에서 매 반복 호출되므로 임계구역 진입 전에 빠진다
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  usbHidUpdateRoute();

  if (qbufferAvailable(&report_kbd_q) > 0 && !usbHidKbdEpBusy())
  {
    kbd_report_info_t report_info;

    qbufferRead(&report_kbd_q, (uint8_t *)&report_info, 1);

    // 전송이 거절되면(미CONFIGURED) DataIn 이 오지 않으므로 측정 플래그를 세우지
    // 않는다. 세워두면 다음 리포트의 DataIn 이 죽은 리포트의 시각으로 계산한다.
    if (usbHidTransmitKbd(route_is_boot, report_info.buf))
    {
      key_time_pre      = report_info.time_pre;
      key_time_req      = true;
      key_time_raw_req  = report_info.raw_req;
      key_time_raw_pre  = report_info.raw_pre;
      key_time_proc_req = report_info.proc_req;
      key_time_proc_pre = report_info.proc_pre;
      if (report_info.qmk_req)
      {
        key_break_qmk_cyc = report_info.qmk_cyc;
      }

      rate_time_req = true;
      rate_time_pre = micros();
    }
  }

  if (qbufferAvailable(&report_exk_q) > 0 && !exk_ep_busy)
  {
    exk_report_info_t report_info;

    qbufferRead(&report_exk_q, (uint8_t *)&report_info, 1);
    memcpy(hid_buf_exk, report_info.buf, report_info.len);
    extk_kbd_in_flight = false;   // 이 EP 에 실린 것은 키 리포트가 아니다
    USBD_HID_SendReportEXK((uint8_t *)hid_buf_exk, report_info.len);
  }

  __set_PRIMASK(primask);
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  timer_cnt++;
  timer_end = micros()-rate_time_sof_pre;

  usbHidFlush();

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

  if (args->argc == 1 && args->isStr(0, "proto") == true)
  {
    cliPrintf("protocol : %d (%s)\n", usbHidGetProtocol(), usbHidGetProtocol() ? "report" : "boot");
    cliPrintf("route    : %s\n", usbHidIsBootRoute() ? "IF0 boot 8byte 6KRO" : "IF2 ext 20KRO");
    ret = true;
  }

  // 부트 프로토콜은 BIOS 만 요구하므로 책상에서는 이렇게 흉내 내야 시험이 된다.
  if (args->argc == 2 && args->isStr(0, "proto") == true)
  {
    usbHidSetProtocolTest((uint8_t)args->getData(1));
    cliPrintf("protocol -> %d (%s)\n", usbHidGetProtocol(), usbHidGetProtocol() ? "report" : "boot");
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
    uint16_t time_max[4] = {0, 0, 0, 0};
    uint16_t time_min[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    uint16_t time_sum[4] = {0, 0, 0, 0};

    //          usb=큐잉→TX,  raw=접점→TX,  pre=접점→큐잉,  proc=확정스캔→큐잉(디바운스 제외 CPU)
    cliPrintf("      usb    raw    pre    proc\n");
    for (int i = 0; i < key_time_cnt; i++)
    {
      if (key_time_cnt == KEY_TIME_LOG_MAX)
        index = (key_time_idx + i) % KEY_TIME_LOG_MAX;
      else
        index = i;

      cliPrintf("%2d: %5d %5d %5d %5d us\n",
                i,
                key_time_log[index],
                key_time_raw_log[index],
                key_time_pre_log[index],
                key_time_proc_log[index]);

      for (int j=0; j<4; j++)
      {
        uint16_t data;

        if (j == 0)
          data = key_time_log[index];
        else if (j == 1)
          data = key_time_raw_log[index];
        else if (j == 2)
          data = key_time_pre_log[index];
        else
          data = key_time_proc_log[index];

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
      cliPrintf("      usb    raw    pre    proc\n");
      cliPrintf("avg : %5d %5d %5d %5d us\n",
                time_sum[0] / key_time_cnt,
                time_sum[1] / key_time_cnt,
                time_sum[2] / key_time_cnt,
                time_sum[3] / key_time_cnt);
      cliPrintf("max : %5d %5d %5d %5d us\n", time_max[0], time_max[1], time_max[2], time_max[3]);
      cliPrintf("min : %5d %5d %5d %5d us\n", time_min[0], time_min[1], time_min[2], time_min[3]);
    }

    // proc 세분 (최근 keystroke, DWT 사이클) = scan(캡처read) + decode(전치) + qmk(스캔종료→큐잉)
    {
      uint32_t mhz = SystemCoreClock / 1000000;
      cliPrintf("\nproc breakdown (last, DWT):\n");
      cliPrintf("  scan   : %4d cyc (%d ns)\n", key_break_scan_cyc,   key_break_scan_cyc   * 1000 / mhz);
      cliPrintf("  decode : %4d cyc (%d ns)\n", key_break_decode_cyc, key_break_decode_cyc * 1000 / mhz);
      cliPrintf("  qmk    : %4d cyc (%d ns)\n", key_break_qmk_cyc,    key_break_qmk_cyc    * 1000 / mhz);
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usbhid info\n");
    cliPrintf("usbhid proto\n");
    cliPrintf("usbhid proto 0:boot 1:report\n");
    cliPrintf("usbhid rate\n");
    cliPrintf("usbhid rate his\n");
    cliPrintf("usbhid log\n");
    cliPrintf("usbhid log clear\n");
  }
}
#endif
