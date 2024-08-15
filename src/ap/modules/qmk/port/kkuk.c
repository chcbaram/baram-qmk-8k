#include "quantum.h"

#ifdef KKUK_ENABLE


#define KKUK_TIME_UNIT      10



enum via_qmk_kill_switch_value {
    id_qmk_kkuk_enable      = 1,
    id_qmk_kkuk_delay_time  = 2,
    id_qmk_kkuk_repeat_time = 3,
};


typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t  enable : 2;
    uint8_t  mode   : 6;
    uint8_t  repeat_time;
    uint8_t  delay_time;
  };

} kkuk_config_t;

_Static_assert(sizeof(kkuk_config_t) == sizeof(uint32_t), "EECONFIG out of spec.");


static void via_qmk_kkuk_get_value(uint8_t *data);
static void via_qmk_kkuk_set_value(uint8_t *data);
static void via_qmk_kkuk_save(void);


static kkuk_config_t kkuk_config;

EECONFIG_DEBOUNCE_HELPER(kkuk,   EECONFIG_USER_KKUK,   kkuk_config);


static bool is_req_repeqt = false;
static bool is_kkuk_mode = false;

static uint32_t pre_time;
static uint32_t pre_time_delay;
static uint8_t key_cnt = 0;
static report_keyboard_t last_report;



void kkuk_init(void)
{
  eeconfig_init_kkuk();
  if (kkuk_config.mode != 1)
  {
    kkuk_config.mode        = 1;
    kkuk_config.enable      = false;
    kkuk_config.delay_time  = 200;
    kkuk_config.repeat_time = 80;
    eeconfig_flush_kkuk(true);
  }

  logPrintf("[ON] KKUK\n");
}

void kkuk_idle(void)
{
  enum
  {
    KEY_ST_IDLE,
    KEY_ST_REPEAT
  };

  static uint8_t  state = KEY_ST_IDLE;
  static uint8_t  pre_cnt = 0;
  static uint16_t delay_time;
  static uint16_t repeat_time;


  if (!kkuk_config.enable)
  {
    return;
  }

  delay_time  = kkuk_config.delay_time;
  repeat_time = kkuk_config.repeat_time;

  if (!is_kkuk_mode)
  {
    if (key_cnt >= 2  && millis()-pre_time_delay >= delay_time)
    {
      is_kkuk_mode = true;
      pre_time = millis() + 10;
    }
  }
  else
  {
    if (key_cnt == 0)
    {
      is_kkuk_mode = false;
    }
  }

  if (millis()-pre_time >= repeat_time)
  {
    pre_time = millis();

    if (is_kkuk_mode)
    {
      if (key_cnt >= 2)
      {
        is_req_repeqt = true;
      }   
      if (key_cnt == 1 && pre_cnt == 2)
      {
        is_req_repeqt = true;
      }
    }
    
    pre_cnt = key_cnt;


    if (is_req_repeqt && state == KEY_ST_IDLE)
    {
      is_req_repeqt = false;
      state = KEY_ST_REPEAT;
    }

    switch(state)
    {
      case KEY_ST_REPEAT:        
        memcpy(&last_report, keyboard_report, sizeof(report_keyboard_t));
        clear_keys();
        send_keyboard_report();
        memcpy(keyboard_report, &last_report, sizeof(report_keyboard_t));
        send_keyboard_report();
        state = KEY_ST_IDLE;
        break;
    }
  }
}

bool kkuk_process(uint16_t keycode, keyrecord_t *record)
{
  if (!kkuk_config.enable)
  {
    return true;
  }

#ifdef KILL_SWITCH_ENABLE
  if (kill_switch_is_use(keycode))
  {
    return true;
  }
#endif

  if (IS_BASIC_KEYCODE(keycode))
  {
    if (record->event.pressed)
    {
      key_cnt++;
    }
    else
    {
      key_cnt = key_cnt > 0 ? (key_cnt - 1):(key_cnt + 0);
    }
    pre_time_delay = millis();
  }
  // cliPrintf("cnt %d\n", key_cnt);
  return true;
}


void via_qmk_kkuk_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_kkuk_set_value(value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_kkuk_get_value(value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        via_qmk_kkuk_save();
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_kkuk_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_kkuk_enable:
      {
        value_data[0] = kkuk_config.enable;
        break;
      }    
    case id_qmk_kkuk_delay_time:
      {
        value_data[0] = kkuk_config.delay_time / KKUK_TIME_UNIT;
        break;
      }         
    case id_qmk_kkuk_repeat_time:
      {
        value_data[0] = kkuk_config.repeat_time / KKUK_TIME_UNIT;
        break;
      }
  }
}

void via_qmk_kkuk_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_kkuk_enable:
      {
        kkuk_config.enable = value_data[0];
        break;
      }
    case id_qmk_kkuk_delay_time:
      {
        kkuk_config.delay_time = value_data[0] * KKUK_TIME_UNIT;
        break;
      }      
    case id_qmk_kkuk_repeat_time:
      {
        kkuk_config.repeat_time = value_data[0] * KKUK_TIME_UNIT;
        break;
      }
  }
}

void via_qmk_kkuk_save(void)
{
  eeconfig_flush_kkuk(true);
}

#endif