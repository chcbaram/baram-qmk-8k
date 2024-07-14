#include "quantum.h"
#include "led_port.h"







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

  // Return the unhandled state
  *command_id = id_unhandled;
}