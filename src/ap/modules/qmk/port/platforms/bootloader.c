#include "bootloader.h"
#include "eeprom.h"

void bootloader_jump(void)
{
  resetToBoot();
}

void mcu_reset(void)
{
  for (int i=0; i<32; i++)
  {
    eeprom_update();
  }
  resetToReset();
}