#include "qmk.h"
#include "qmk/port/port.h"


static void cliQmk(cli_args_t *args);
static void suspend_task(void);

static bool is_suspended = false;




bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  keyboard_setup();
  keyboard_init();

  
  is_suspended = usbIsSuspended();

  logPrintf("[  ] qmkInit()\n");
  logPrintf("     MATRIX_ROWS : %d\n", MATRIX_ROWS);
  logPrintf("     MATRIX_COLS : %d\n", MATRIX_COLS);
  logPrintf("     DEBOUNCE    : %d\n", DEBOUNCE);

  cliAdd("qmk", cliQmk);
  return true;
}

void keyboard_post_init_user(void)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_init();
#endif
}

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
  suspend_task();
}

void suspend_task(void)
{
  bool is_suspended_cur;

  is_suspended_cur = usbIsSuspended();
  if (is_suspended_cur != is_suspended)
  {
    if (is_suspended_cur)
    {
      suspend_power_down();
    }
    else
    {
      suspend_wakeup_init();
    }

    is_suspended = is_suspended_cur;
  }

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