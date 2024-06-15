#include "qmk.h"
#include "qmk/port/port.h"


static void cliQmk(cli_args_t *args);




bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  keyboard_setup();
  keyboard_init();

  logPrintf("[  ] qmkInit()\n");
  logPrintf("     MATRIX_ROWS : %d\n", MATRIX_ROWS);
  logPrintf("     MATRIX_COLS : %d\n", MATRIX_COLS);
  logPrintf("     DEBOUNCE    : %d\n", DEBOUNCE);

  cliAdd("qmk", cliQmk);
  return true;
}

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
}

void cliQmk(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 2 && args->isStr(0, "clear") && args->isStr(1, "eeprom"))
  {
    eeconfig_init();
    cliPrintf("Clearing EEPROM\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("qmk info\n");
    cliPrintf("qmk clear eeprom\n");
  }
}