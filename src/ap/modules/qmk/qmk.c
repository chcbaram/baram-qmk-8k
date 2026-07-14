#include "qmk.h"
#include "qmk/port/port.h"
#include "usbd_hid.h"


static void cliQmk(cli_args_t *args);
static void idle_task(void);

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

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
  idle_task();
  usbLinkFramePoll();   // USB SOF 순단 감지(FNSOF 폴링, 전송 영향 없음)
}

void keyboard_post_init_user(void)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_init();
#endif
#ifdef KKUK_ENABLE
  kkuk_init();
#endif
#ifdef DEBOUNCE_RUNTIME
  debounce_cfg_init();
#endif
#ifdef HOLD_OKP_RUNTIME
  hold_okp_init();
#endif
}

bool process_record_user(uint16_t keycode, keyrecord_t *record)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_process(keycode, record);
#endif
#ifdef KKUK_ENABLE
  kkuk_process(keycode, record);
#endif
  return true;
}

void idle_task(void)
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

#ifdef KKUK_ENABLE
  kkuk_idle();
#endif

  chattering_task();   // 채터링 점검 워치독(웹 명령 끊기면 자동 종료)
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