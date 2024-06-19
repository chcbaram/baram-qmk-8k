#include "ap.h"
#include "qmk/qmk.h"


void cliUpdate(void);




void apInit(void)
{  
  cliOpen(HW_UART_CH_CLI, 115200);
  qmkInit();
  
  logBoot(false);
}

void apMain(void)
{
  uint32_t pre_time;
  bool is_led_on = true;


  ledOn(_DEF_LED1);

  pre_time = millis();
  while(1)
  {
    if (is_led_on && millis()-pre_time >= 500)
    {
      is_led_on = false;
      ledOff(_DEF_LED1);
    }

    cliUpdate();
    qmkUpdate();
  }
}

void cliUpdate(void)
{
  static uint8_t cli_ch = HW_UART_CH_CLI; 

  if (usbIsOpen() && usbGetType() == USB_CON_CLI)
  {
    cli_ch = HW_UART_CH_USB;
  }
  else
  {
    cli_ch = HW_UART_CH_CLI;
  }
  if (cli_ch != cliGetPort())
  {
    if (cli_ch == HW_UART_CH_USB)
      logPrintf("\nCLI To USB\n");
    else
      logPrintf("\nCLI To UART\n");
    cliOpen(cli_ch, 0);
  }

  cliMain();
}