#include "bootloader.h"


void bootloader_jump(void)
{
  resetToBoot();
}

void mcu_reset(void)
{
  resetToReset();
}