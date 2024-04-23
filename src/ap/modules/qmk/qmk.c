#include "qmk.h"
#include "qmk/port/port.h"



bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  keyboard_setup();
  keyboard_init();

  return true;
}

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
}
