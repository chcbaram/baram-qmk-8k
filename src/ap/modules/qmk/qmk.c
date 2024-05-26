#include "qmk.h"
#include "qmk/port/port.h"



bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  keyboard_setup();
  keyboard_init();

  logPrintf("[  ] qmkInit(\n");
  logPrintf("     MATRIX_ROWS : %d\n", MATRIX_ROWS);
  logPrintf("     MATRIX_COLS : %d\n", MATRIX_COLS);
  logPrintf("     DEBOUNCE    : %d\n", DEBOUNCE);

  return true;
}

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
}
