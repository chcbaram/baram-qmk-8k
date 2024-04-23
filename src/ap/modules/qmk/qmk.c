#include "qmk.h"
#include "raw_hid.h"
#include "via.h"
#include "eeconfig.h"



void qmkViaHidreceive(uint8_t *data, uint8_t length);
void viaHidCmdPrint(uint8_t *data, uint8_t length, bool is_resp);


bool qmkInit(void)
{
  keyboard_setup();
  keyboard_init();


  usbHidSetViaReceiveFunc(qmkViaHidreceive);

  return true;
}

void qmkUpdate(void)
{
  keyboard_task();
}

void qmkViaHidreceive(uint8_t *data, uint8_t length)
{
  viaHidCmdPrint(data, length, false);

  raw_hid_receive(data, length);
}


static const char *command_id_str[] =
{
  [id_get_protocol_version]                 = "id_get_protocol_version",
  [id_get_keyboard_value]                   = "id_get_keyboard_value",
  [id_set_keyboard_value]                   = "id_set_keyboard_value",
  [id_dynamic_keymap_get_keycode]           = "id_dynamic_keymap_get_keycode",
  [id_dynamic_keymap_set_keycode]           = "id_dynamic_keymap_set_keycode",
  [id_dynamic_keymap_reset]                 = "id_dynamic_keymap_reset",
  [id_custom_set_value]                     = "id_custom_set_value",
  [id_custom_get_value]                     = "id_custom_get_value",
  [id_custom_save]                          = "id_custom_save",
  [id_eeprom_reset]                         = "id_eeprom_reset",
  [id_bootloader_jump]                      = "id_bootloader_jump",
  [id_dynamic_keymap_macro_get_count]       = "id_dynamic_keymap_macro_get_count",
  [id_dynamic_keymap_macro_get_buffer_size] = "id_dynamic_keymap_macro_get_buffer_size",
  [id_dynamic_keymap_macro_get_buffer]      = "id_dynamic_keymap_macro_get_buffer",
  [id_dynamic_keymap_macro_set_buffer]      = "id_dynamic_keymap_macro_set_buffer",
  [id_dynamic_keymap_macro_reset]           = "id_dynamic_keymap_macro_reset",
  [id_dynamic_keymap_get_layer_count]       = "id_dynamic_keymap_get_layer_count",
  [id_dynamic_keymap_get_buffer]            = "id_dynamic_keymap_get_buffer",
  [id_dynamic_keymap_set_buffer]            = "id_dynamic_keymap_set_buffer",
  [id_dynamic_keymap_get_encoder]           = "id_dynamic_keymap_get_encoder",
  [id_dynamic_keymap_set_encoder]           = "id_dynamic_keymap_set_encoder",
  [id_unhandled]                            = "id_unhandled",
};

void viaHidCmdPrint(uint8_t *data, uint8_t length, bool is_resp)
{
  uint8_t *command_id   = &(data[0]);
  uint8_t *command_data = &(data[1]);  
  uint8_t data_len = length - 1;


  logPrintf("[%s] id : 0x%02X, 0x%02X, len %d,  %s",
            is_resp == true ? "OK" : "  ",
            *command_id,
            command_data[0],
            length,
            command_id_str[*command_id]);

  if (is_resp == true)
  {
    logPrintf("\n");
    return;
  }

  for (int i=0; i<data_len; i++)
  {
    if (i%8 == 0)
      logPrintf("\n     ");
    logPrintf("0x%02X ", command_data[i]);
  }
  logPrintf("\n");  
}