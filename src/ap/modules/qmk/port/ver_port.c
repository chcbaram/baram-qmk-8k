#include "ver_port.h"





enum via_qmk_ver_item {
    id_qmk_ver_yy  = 1,
    id_qmk_ver_mm  = 2,
    id_qmk_ver_dd  = 3,
    id_qmk_ver_rv  = 4,
};

static void via_qmk_ver_get_value(uint8_t *data);

static const char *ver_str = _DEF_FIRMWATRE_VERSION;




void via_qmk_version(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_ver_get_value(value_id_and_data);
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

void via_qmk_ver_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);
  char ver_buf[3] = {0,};

  switch (*value_id)
  {
    case id_qmk_ver_yy:
      {
        ver_buf[0] = ver_str[1];
        ver_buf[1] = ver_str[2];
        value_data[0] = ((uint8_t)strtoul((const char * )ver_buf, (char **)NULL, (int)10)) - 24;
        break;
      }    
    case id_qmk_ver_mm:
      {
        ver_buf[0] = ver_str[3];
        ver_buf[1] = ver_str[4];
        value_data[0] = ((uint8_t)strtoul((const char * )ver_buf, (char **)NULL, (int)10)) - 1;
        break;
      }
    case id_qmk_ver_dd:
      {
        ver_buf[0] = ver_str[5];
        ver_buf[1] = ver_str[6];
        value_data[0] = ((uint8_t)strtoul((const char * )ver_buf, (char **)NULL, (int)10)) - 1;
        break;
      }
    case id_qmk_ver_rv:
      {
        ver_buf[0] = '0';
        ver_buf[1] = ver_str[8];
        value_data[0] = ((uint8_t)strtoul((const char * )ver_buf, (char **)NULL, (int)10)) - 1;
        break;
      }
  }
}
