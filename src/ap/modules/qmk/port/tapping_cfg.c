#include "quantum.h"

#ifdef TAPPING_TERM_RUNTIME


#define TAPPING_CFG_MAGIC     0xA5

// VIA 슬라이더 범위와 같아야 한다 (VIA JSON "Tapping Term" options).
// 50ms 미만은 일반 타이핑 속도에서도 탭이 홀드로 바뀌고, 500ms 초과는 홀드 반응이 너무 늦다.
#define TAPPING_TERM_MIN      50
#define TAPPING_TERM_MAX      500
#define TAPPING_TERM_DEFAULT  TAPPING_TERM


enum via_qmk_tapping_value {
    id_qmk_tapping_term = 1,
};


typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint16_t term;      // 탭핑 텀(ms)
    uint8_t  magic;     // TAPPING_CFG_MAGIC 이면 초기화됨
    uint8_t  reserved;
  };

} tapping_cfg_t;

_Static_assert(sizeof(tapping_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");


static void via_qmk_tapping_get_value(uint8_t *data);
static void via_qmk_tapping_set_value(uint8_t *data);
static void via_qmk_tapping_save(void);
static uint16_t tapping_term_clamp(uint16_t term);


static tapping_cfg_t tapping_cfg_config;

EECONFIG_DEBOUNCE_HELPER(tapping_cfg, EECONFIG_USER_TAPPING, tapping_cfg_config);


void tapping_cfg_init(void)
{
  eeconfig_init_tapping_cfg();

  // EEPROM 미초기화(0x00) 또는 손상값(0xFF 등) 방어 → 기본값(TAPPING_TERM)으로 클램프
  if (tapping_cfg_config.magic != TAPPING_CFG_MAGIC ||
      tapping_cfg_config.term < TAPPING_TERM_MIN ||
      tapping_cfg_config.term > TAPPING_TERM_MAX)
  {
    tapping_cfg_config.term     = TAPPING_TERM_DEFAULT;
    tapping_cfg_config.magic    = TAPPING_CFG_MAGIC;
    tapping_cfg_config.reserved = 0;
    eeconfig_flush_tapping_cfg(true);
  }

  logPrintf("[OK] TAPPING TERM RUNTIME (term:%d ms)\n", tapping_cfg_config.term);
}


// TAPPING_TERM_PER_KEY 콜백 — weak 기본(TAPPING_TERM 상수) 을 override 한다.
// Mod-Tap/Layer-Tap(action_tapping.c) 과 Space Cadet 이 모두 GET_TAPPING_TERM 으로 이 값을 읽는다.
uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record)
{
  return tapping_cfg_config.term;
}


// QUICK_TAP_TERM_PER_KEY 콜백 — 퀵탭텀도 탭핑 텀을 따라가게 한다.
// QUICK_TAP_TERM 은 미지정 시 컴파일 때 TAPPING_TERM(200) 으로 굳어서, 탭핑 텀만 150 으로
// 내리면 QMK 가 전제하는 "퀵탭텀 <= 탭핑 텀" 이 깨진다 (qmk-link 에서 겪은 것).
uint16_t get_quick_tap_term(uint16_t keycode, keyrecord_t *record)
{
  return tapping_cfg_config.term;
}


void via_qmk_tapping_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_tapping_set_value(value_id_and_data);
        break;
      }

    case id_custom_get_value:
      {
        via_qmk_tapping_get_value(value_id_and_data);
        break;
      }

    case id_custom_save:
      {
        via_qmk_tapping_save();
        break;
      }

    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_tapping_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_tapping_term:
      {
        // range 최댓값이 255 를 넘으면 VIA 는 2바이트(big-endian) 로 주고받는다.
        value_data[0] = (uint8_t)(tapping_cfg_config.term >> 8);
        value_data[1] = (uint8_t)(tapping_cfg_config.term & 0xFF);
        break;
      }
  }
}

void via_qmk_tapping_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_tapping_term:
      {
        uint16_t term = ((uint16_t)value_data[0] << 8) | value_data[1];

        tapping_cfg_config.term = tapping_term_clamp(term);   // 저장 전에도 즉시 적용(콜백이 config 직접 참조)
        break;
      }
  }
}

void via_qmk_tapping_save(void)
{
  eeconfig_flush_tapping_cfg(true);
}

uint16_t tapping_term_clamp(uint16_t term)
{
  uint16_t ret = term;

  if (ret < TAPPING_TERM_MIN)
  {
    ret = TAPPING_TERM_MIN;
  }
  if (ret > TAPPING_TERM_MAX)
  {
    ret = TAPPING_TERM_MAX;
  }

  return ret;
}

#endif
