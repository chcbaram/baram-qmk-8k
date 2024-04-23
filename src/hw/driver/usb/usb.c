/*
 * usb.c
 *
 *  Created on: 2018. 3. 16.
 *      Author: HanCheol Cho
 */


#include "usb.h"
#include "cdc.h"
#include "cli.h"


#ifdef _USE_HW_USB
#include "usbd_cmp.h"
#include "usbd_hid.h"


static bool is_init = false;
static UsbMode_t is_usb_mode = USB_NON_MODE;

USBD_HandleTypeDef USBD_Device;
extern PCD_HandleTypeDef hpcd_USB_OTG_HS;

extern USBD_DescriptorsTypeDef VCP_Desc;
extern USBD_DescriptorsTypeDef MSC_Desc;
extern USBD_DescriptorsTypeDef HID_Desc;
extern USBD_DescriptorsTypeDef CMP_Desc;

static USBD_DescriptorsTypeDef *p_desc = NULL;

static uint8_t hid_ep_tbl[] = {
  HID_EPIN_ADDR, 
  HID_VIA_EP_IN, 
  HID_VIA_EP_OUT};
static uint8_t cdc_ep_tbl[] = {
  CDC_IN_EP, 
  CDC_OUT_EP, 
  CDC_CMD_EP};


#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif




bool usbInit(void)
{
#ifdef _USE_HW_CLI
  cliAdd("usb", cliCmd);
#endif
  return true;
}

bool usbBegin(UsbMode_t usb_mode)
{
  is_init = true;

  if (usb_mode == USB_CDC_MODE)
  {
    #if HW_USB_CDC == 1    
    /* Init Device Library */
    USBD_Init(&USBD_Device, &VCP_Desc, DEVICE_HS);

    /* Add Supported Class */
    USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);

    /* Add CDC Interface Class */
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);

    /* Start Device Process */
    USBD_Start(&USBD_Device);

    // HAL_PWREx_EnableUSBVoltageDetector();

    is_usb_mode = USB_CDC_MODE;
    
    p_desc = &VCP_Desc;
    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_CDC\r\n");
    #endif
  }
  else if (usb_mode == USB_MSC_MODE)
  {
    #if HW_USB_MSC == 1
    /* Init Device Library */
    USBD_Init(&USBD_Device, &MSC_Desc, DEVICE_HS);

    /* Add Supported Class */
    USBD_RegisterClass(&USBD_Device, USBD_MSC_CLASS);

    /* Add Storage callbacks for MSC Class */
    USBD_MSC_RegisterStorage(&USBD_Device, &USBD_DISK_fops);

    /* Start Device Process */
    USBD_Start(&USBD_Device);

    HAL_PWREx_EnableUSBVoltageDetector();

    is_usb_mode = USB_MSC_MODE;

    p_desc = &MSC_Desc;
    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_MSC\r\n");
    #endif
  }
  else if (usb_mode == USB_HID_MODE)
  {
    #if HW_USB_HID == 1
    /* Init Device Library */
    USBD_Init(&USBD_Device, &HID_Desc, DEVICE_HS);

    /* Add Supported Class */
    USBD_RegisterClass(&USBD_Device, USBD_HID_CLASS);

    /* Start Device Process */
    USBD_Start(&USBD_Device);

    is_usb_mode = USB_HID_MODE;
    
    p_desc = &HID_Desc;
    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_HID\r\n");
    #endif
  }  
  else if (usb_mode == USB_CMP_MODE)
  { 
    #if HW_USB_CMP == 1
    USBD_Init(&USBD_Device, &CMP_Desc, DEVICE_HS);


    /* Add Supported Class */
    USBD_RegisterClassComposite(&USBD_Device, USBD_HID_CLASS, CLASS_TYPE_HID, hid_ep_tbl);
    USBD_RegisterClassComposite(&USBD_Device, USBD_CDC_CLASS, CLASS_TYPE_CDC, cdc_ep_tbl);

    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);

    /* Start Device Process */
    USBD_Start(&USBD_Device);

    is_usb_mode = USB_CDC_MODE;
    
    p_desc = &CMP_Desc;
    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_CMP\r\n");
    #endif
  }  
  else
  {
    is_init = false;

    logPrintf("[NG] usbBegin()\n");
  }

  return is_init;
}

void usbDeInit(void)
{
  if (is_init == true)
  {
    USBD_DeInit(&USBD_Device);
  }
}

bool usbIsOpen(void)
{
  #if HW_USB_CDC == 1
  return cdcIsConnect();
  #else
  return false;
  #endif
}

bool usbIsConnect(void)
{
  if (USBD_Device.pClassData == NULL)
  {
    return false;
  }
  if (USBD_Device.dev_state != USBD_STATE_CONFIGURED)
  {
    return false;
  }
  if (USBD_Device.dev_config == 0)
  {
    return false;
  }
  if (USBD_is_connected() == false)
  {
    return false;
  }
  
  return true;
}

UsbMode_t usbGetMode(void)
{
  return is_usb_mode;
}

UsbType_t usbGetType(void)
{
#if HW_USB_CDC == 1  
  return (UsbType_t)cdcGetType();
#elif HW_USE_KBD == 1
  return USB_CON_KBD;
#else
  return USB_CON_CDC;
#endif
}

void OTG_HS_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);
}


#ifdef _USE_HW_CLI
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint8_t *p_data;
    uint16_t len = 0;


    if (p_desc != NULL)
    {
      p_data = p_desc->GetDeviceDescriptor(USBD_SPEED_HIGH, &len);
      vid = (p_data[ 9]<<8)|(p_data[ 8]<<0);
      pid = (p_data[11]<<8)|(p_data[10]<<0);
    }

    cliPrintf("USB PID     : 0x%04X\n", vid);
    cliPrintf("USB VID     : 0x%04X\n", pid);

    while(cliKeepLoop())
    {
      cliPrintf("USB Mode    : %d\n", usbGetMode());
      cliPrintf("USB Type    : %d\n", usbGetType());
      cliPrintf("USB Connect : %d\n", usbIsConnect());
      cliPrintf("USB Open    : %d\n", usbIsOpen());
      cliPrintf("\x1B[%dA", 4);
      delay(100);
    }
    cliPrintf("\x1B[%dB", 4);

    ret = true;
  }
#if HW_USB_CDC == 1
  if (args->argc == 1 && args->isStr(0, "tx") == true)
  {
    uint32_t pre_time;
    uint32_t tx_cnt = 0;
    uint32_t sent_len = 0;

    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("tx : %d KB/s\n", tx_cnt/1024);
        tx_cnt = 0;
      }
      sent_len = cdcWrite((uint8_t *)"123456789012345678901234567890\n", 31);
      tx_cnt += sent_len;
    }
    cliPrintf("\x1B[%dB", 2);

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "rx") == true)
  {
    uint32_t pre_time;
    uint32_t rx_cnt = 0;
    uint32_t rx_len;

    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("rx : %d KB/s\n", rx_cnt/1024);
        rx_cnt = 0;
      }

      rx_len = cdcAvailable();

      for (int i=0; i<rx_len; i++)
      {
        cdcRead();
      }

      rx_cnt += rx_len;
    }
    cliPrintf("\x1B[%dB", 2);

    ret = true;
  }
#endif

  if (ret == false)
  {
    cliPrintf("usb info\n");
    #if HW_USB_CDC == 1
    cliPrintf("usb tx\n");
    cliPrintf("usb rx\n");
    #endif
  }
}
#endif

#endif