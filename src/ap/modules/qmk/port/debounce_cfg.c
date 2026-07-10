#include "quantum.h"

#ifdef DEBOUNCE_RUNTIME

#include "debounce_runtime.h"


#define DEBOUNCE_TIME_MIN     5
#define DEBOUNCE_TIME_MAX     40
#define DEBOUNCE_TIME_DEFAULT 20


enum via_qmk_debounce_value {
    id_qmk_debounce_type = 1,
    id_qmk_debounce_time = 2,
};


typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t type;   // 0 = GAMING(sym_eager_pk), 1 = TYPING(sym_defer_pk)
    uint8_t time;   // 디바운스 시간(ms)
  };

} debounce_cfg_t;

_Static_assert(sizeof(debounce_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");


static void via_qmk_debounce_get_value(uint8_t *data);
static void via_qmk_debounce_set_value(uint8_t *data);
static void via_qmk_debounce_save(void);


static debounce_cfg_t debounce_cfg_config;

EECONFIG_DEBOUNCE_HELPER(debounce_cfg, EECONFIG_USER_DEBOUNCE, debounce_cfg_config);


void debounce_cfg_init(void)
{
  bool need_flush = false;

  eeconfig_init_debounce_cfg();

  // EEPROM 미초기화(0x00) 또는 손상값(0xFF 등) 방어 → 기본값으로 클램프
  if (debounce_cfg_config.type > DEBOUNCE_TYPE_TYPING)
  {
    debounce_cfg_config.type = DEBOUNCE_TYPE_GAMING;
    need_flush = true;
  }
  if (debounce_cfg_config.time < DEBOUNCE_TIME_MIN || debounce_cfg_config.time > DEBOUNCE_TIME_MAX)
  {
    debounce_cfg_config.time = DEBOUNCE_TIME_DEFAULT;
    need_flush = true;
  }

  if (need_flush)
  {
    eeconfig_flush_debounce_cfg(true);
  }

  debounce_set_type(debounce_cfg_config.type);
  debounce_set_time(debounce_cfg_config.time);

  logPrintf("[ON] DEBOUNCE RUNTIME (type:%d time:%d)\n",
            debounce_cfg_config.type, debounce_cfg_config.time);
}


void via_qmk_debounce_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_debounce_set_value(value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_debounce_get_value(value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        via_qmk_debounce_save();
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_debounce_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_debounce_type:
      {
        value_data[0] = debounce_cfg_config.type;
        break;
      }
    case id_qmk_debounce_time:
      {
        value_data[0] = debounce_cfg_config.time;
        break;
      }
  }
}

void via_qmk_debounce_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_debounce_type:
      {
        debounce_cfg_config.type = value_data[0];
        debounce_set_type(debounce_cfg_config.type);   // 저장 전에도 즉시 적용
        break;
      }
    case id_qmk_debounce_time:
      {
        debounce_cfg_config.time = value_data[0];
        debounce_set_time(debounce_cfg_config.time);   // 저장 전에도 즉시 적용
        break;
      }
  }
}

void via_qmk_debounce_save(void)
{
  eeconfig_flush_debounce_cfg(true);
}

#endif
