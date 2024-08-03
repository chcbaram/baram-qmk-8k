#include "hw.h"



extern uint32_t _fw_flash_begin;

volatile const firm_ver_t firm_ver __attribute__((section(".version"))) = 
{
  .magic_number = VERSION_MAGIC_NUMBER,
  .version_str  = _DEF_FIRMWATRE_VERSION,
  .name_str     = KBD_NAME,
  .firm_addr    = (uint32_t)&_fw_flash_begin
};



bool hwInit(void)
{  
  cliInit();
  logInit();  
  ledInit();
  microsInit();

  uartInit();
  for (int i=0; i<HW_UART_MAX_CH; i++)
  {
    uartOpen(i, 115200);
  }

  logOpen(HW_LOG_CH, 115200);
  logPrintf("\r\n[ Firmware Begin... ]\r\n");
  logPrintf("Booting..Name \t\t: %s\r\n", _DEF_BOARD_NAME);
  logPrintf("Booting..KBD  \t\t: %s\r\n", KBD_NAME);  
  logPrintf("Booting..Ver  \t\t: %s\r\n", _DEF_FIRMWATRE_VERSION);  
  logPrintf("Booting..Clock\t\t: %d Mhz\r\n", (int)HAL_RCC_GetSysClockFreq()/1000000);
  logPrintf("Booting..Date \t\t: %s\r\n", __DATE__); 
  logPrintf("Booting..Time \t\t: %s\r\n", __TIME__); 
  logPrintf("\n");

  rtcInit();
  resetInit();
  i2cInit();
  eepromInit();
  spiInit();
  flashInit();
  keysInit();
  #ifdef _USE_HW_WS2812
  ws2812Init();
  #endif
  #ifdef _USE_HW_ENCODER
  encoderInit();
  #endif

  cdcInit();
  usbInit();
  #if HW_USB_CMP
  usbBegin(USB_CMP_MODE);
  #else
  usbBegin(USB_HID_MODE);
  #endif
  loaderInit();
  
  return true;
}