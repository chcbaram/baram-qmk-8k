#include "quantum.h"

#ifdef HOLD_OKP_RUNTIME


#define HOLD_OKP_MAGIC 0x5A


enum via_qmk_hold_okp_value {
    id_qmk_hold_okp_enable = 1,
};


typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t enable;   // 1 = ON(기본), 0 = OFF
    uint8_t magic;    // HOLD_OKP_MAGIC 이면 초기화됨
  };

} hold_okp_cfg_t;

_Static_assert(sizeof(hold_okp_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");


static void via_qmk_hold_okp_get_value(uint8_t *data);
static void via_qmk_hold_okp_set_value(uint8_t *data);
static void via_qmk_hold_okp_save(void);


static hold_okp_cfg_t hold_okp_config;

EECONFIG_DEBOUNCE_HELPER(hold_okp, EECONFIG_USER_HOLD_OKP, hold_okp_config);


void hold_okp_init(void)
{
  eeconfig_init_hold_okp();

  // EEPROM 미초기화(0x00) 또는 손상값(0xFF 등) 방어 → 기본값(ON)으로 클램프
  if (hold_okp_config.magic != HOLD_OKP_MAGIC)
  {
    hold_okp_config.enable = 1;
    hold_okp_config.magic  = HOLD_OKP_MAGIC;
    eeconfig_flush_hold_okp(true);
  }

  logPrintf("[ON] HOLD_ON_OTHER_KEY_PRESS RUNTIME (enable:%d)\n", hold_okp_config.enable);
}


// HOLD_ON_OTHER_KEY_PRESS_PER_KEY 콜백 — weak 기본(false) 을 override 한다.
bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record)
{
  return hold_okp_config.enable != 0;
}


void via_qmk_hold_okp_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_hold_okp_set_value(value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_hold_okp_get_value(value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        via_qmk_hold_okp_save();
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_hold_okp_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_hold_okp_enable:
      {
        value_data[0] = hold_okp_config.enable;
        break;
      }
  }
}

void via_qmk_hold_okp_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_hold_okp_enable:
      {
        hold_okp_config.enable = value_data[0] ? 1 : 0;   // 저장 전에도 즉시 적용(콜백이 config 직접 참조)
        break;
      }
  }
}

void via_qmk_hold_okp_save(void)
{
  eeconfig_flush_hold_okp(true);
}

#endif
