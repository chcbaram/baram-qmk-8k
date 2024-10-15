#include "led_port.h"
#include "ver_port.h"
#include "sys_port.h"





void via_custom_value_command_kb(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id = &(data[0]);
  uint8_t *channel_id = &(data[1]);


  if (*channel_id == id_qmk_led_caps_channel)
  {
    via_qmk_led_command(0, data, length);
    return;
  }

  if (*channel_id == id_qmk_version)
  {
    via_qmk_version(data, length);
    return;
  }

  if (*channel_id == id_qmk_system)
  {
    via_qmk_system(data, length);
    return;
  }

#ifdef KILL_SWITCH_ENABLE
  if (*channel_id == id_qmk_kill_switch_lr)
  {
    via_qmk_kill_swtich_command(0, data, length);
    return;
  }
  if (*channel_id == id_qmk_kill_switch_ud)
  {
    via_qmk_kill_swtich_command(1, data, length);
    return;
  }
#endif
#ifdef KKUK_ENABLE
  if (*channel_id == id_qmk_kkuk)
  {
    via_qmk_kkuk_command(data, length);
    return;
  }
#endif

  // Return the unhandled state
  *command_id = id_unhandled;
}

