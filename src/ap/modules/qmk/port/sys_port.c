#include "sys_port.h"



enum via_qmk_ver_item {
  id_qmk_system_dfu            = 1,
  id_qmk_system_eep_reset_0    = 2,
  id_qmk_system_eep_reset_1    = 3,
  id_qmk_system_eep_reset_done = 4,
};

static void via_qmk_sys_get_value(uint8_t *data);
static void via_qmk_sys_set_value(uint8_t *data);


static uint8_t eep_reset_confirm = 0x00;




void via_qmk_system(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_sys_set_value(value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_sys_get_value(value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_sys_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_system_dfu:
      {
        resetToBoot();
        value_data[0] = 0;
        break;
      }    
    case id_qmk_system_eep_reset_0:
      {
        eep_reset_confirm |= (value_data[0]<<0);
        break;
      }    
    case id_qmk_system_eep_reset_1:
      {
        eep_reset_confirm |= (value_data[0]<<1);
        break;
      }    
    case id_qmk_system_eep_reset_done:
      {
        eep_reset_confirm |= (value_data[0]<<2);

        if (eep_reset_confirm == 0x07)
        {
          eeprom_req_clean();
        }
        break;
      }    
  }
}

void via_qmk_sys_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_system_dfu:
      {
        value_data[0] = 0;
        break;
      }    
  }
}
