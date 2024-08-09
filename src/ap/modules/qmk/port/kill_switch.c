#include "quantum.h"

#ifdef KILL_SWITCH_ENABLE

#define KILL_DEBUG_LOG            false
#define KILL_SWITCH_MAX_CH        2


enum
{
  KILL_SWITCH_LR = 0,
  KILL_SWITCH_UD,
};

enum via_qmk_kill_switch_value {
    id_qmk_kill_switch_enable      = 1,
    id_qmk_kill_switch_keycode_0   = 2,
    id_qmk_kill_switch_keycode_1   = 3,
};


typedef union
{
  uint64_t raw;

  struct PACKED
  {
    uint8_t  enable;
    uint8_t  mode;
    uint16_t keycode[2];
  };

} kill_switch_config_t;

_Static_assert(sizeof(kill_switch_config_t) == sizeof(uint64_t), "EECONFIG out of spec.");


static void via_qmk_kill_switch_get_value(uint8_t type, uint8_t *data);
static void via_qmk_kill_switch_set_value(uint8_t type, uint8_t *data);
static void via_qmk_kill_switch_save(uint8_t type);


static bool key_pressed_lr[KILL_SWITCH_MAX_CH] = {false, };
static bool key_pressed_ud[KILL_SWITCH_MAX_CH] = {false, };
static kill_switch_config_t kill_switch_config[KILL_SWITCH_MAX_CH];

EECONFIG_DEBOUNCE_HELPER(kill_switch_lr,   EECONFIG_USER_KILL_SWITCH_LR,   kill_switch_config[KILL_SWITCH_LR]);
EECONFIG_DEBOUNCE_HELPER(kill_switch_ud,   EECONFIG_USER_KILL_SWITCH_UD,   kill_switch_config[KILL_SWITCH_UD]);




void kill_switch_init(void)
{
  eeconfig_init_kill_switch_lr();
  if (kill_switch_config[KILL_SWITCH_LR].mode != 1)
  {
    kill_switch_config[KILL_SWITCH_LR].mode = 1;
    kill_switch_config[KILL_SWITCH_LR].enable = false;
    kill_switch_config[KILL_SWITCH_LR].keycode[0] = KC_NO;
    kill_switch_config[KILL_SWITCH_LR].keycode[1] = KC_NO;
    eeconfig_flush_kill_switch_lr(true);
  }

  eeconfig_init_kill_switch_ud();
  if (kill_switch_config[KILL_SWITCH_UD].mode != 1)
  {
    kill_switch_config[KILL_SWITCH_UD].mode = 1;
    kill_switch_config[KILL_SWITCH_UD].enable = false;
    kill_switch_config[KILL_SWITCH_UD].keycode[0] = KC_NO;
    kill_switch_config[KILL_SWITCH_UD].keycode[1] = KC_NO;
    eeconfig_flush_kill_switch_ud(true);
  }

  logPrintf("[ON] KILL SWITCH\n");
}

bool kill_switch_process(uint16_t keycode, keyrecord_t *record)
{
  static kill_switch_config_t *p_cfg_lr = &kill_switch_config[KILL_SWITCH_LR];
  static kill_switch_config_t *p_cfg_ud = &kill_switch_config[KILL_SWITCH_UD];


  if (p_cfg_lr->enable)
  {
    uint16_t next_i;

    for (int i=0; i<2; i++)
    {
      next_i = 1-i;

      if (keycode == p_cfg_lr->keycode[i])
      {
        if (record->event.pressed)
        {
          key_pressed_lr[i] = true;
          if (key_pressed_lr[next_i])
          {
            // unregister_code(p_cfg_lr->keycode[next_i]);
            del_key(p_cfg_lr->keycode[next_i]);
            #if KILL_DEBUG_LOG
            logPrintf(" unregister_code(%s)-0x%04X\n", next_i ? "RIGHT":"LEFT", p_cfg_lr->keycode[next_i]);
            #endif
          }
        }
        else
        {
          key_pressed_lr[i] = false;
          if (key_pressed_lr[next_i])
          {
            // register_code(p_cfg_lr->keycode[next_i]);
            add_key(p_cfg_lr->keycode[next_i]);
            #if KILL_DEBUG_LOG
            logPrintf(" register_code(%s)-0x%04X\n", next_i ? "RIGHT":"LEFT", p_cfg_lr->keycode[next_i]);
            #endif
          }        
        }      
      }
    }
  }

  if (kill_switch_config[KILL_SWITCH_UD].enable)
  {
    uint16_t next_i;

    for (int i=0; i<2; i++)
    {
      next_i = 1-i;

      if (keycode == p_cfg_ud->keycode[i])
      {
        if (record->event.pressed)
        {
          key_pressed_ud[i] = true;
          if (key_pressed_ud[next_i])
          {
            // unregister_code(p_cfg_ud->keycode[next_i]);
            del_key(p_cfg_ud->keycode[next_i]);
            #if KILL_DEBUG_LOG
            logPrintf(" unregister_code(%s)-0x%04X\n", next_i ? "DOWN":"UP", p_cfg_ud->keycode[next_i]);
            #endif
          }
        }
        else
        {
          key_pressed_ud[i] = false;
          if (key_pressed_ud[next_i])
          {
            // register_code(p_cfg_ud->keycode[next_i]);
            add_key(p_cfg_ud->keycode[next_i]);
            #if KILL_DEBUG_LOG
            logPrintf(" register_code(%s)-0x%04X\n", next_i ? "DOWN":"UP", p_cfg_ud->keycode[next_i]);
            #endif
          }        
        }      
      }
    }    
  }

  return true;
}

bool kill_switch_is_use(uint16_t keycode)
{
  bool ret = false;
  static kill_switch_config_t *p_cfg_lr = &kill_switch_config[KILL_SWITCH_LR];
  static kill_switch_config_t *p_cfg_ud = &kill_switch_config[KILL_SWITCH_UD];


  if (p_cfg_lr->enable)
  {
    if (keycode == p_cfg_lr->keycode[0])
    {
      ret = true;
    }
    if (keycode == p_cfg_lr->keycode[1])
    {
      ret = true;
    }
  }
  if (p_cfg_ud->enable)
  {
    if (keycode == p_cfg_ud->keycode[0])
    {
      ret = true;
    }
    if (keycode == p_cfg_ud->keycode[1])
    {
      ret = true;
    }
  }

  return ret;
}

void via_qmk_kill_swtich_command(uint8_t type, uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_kill_switch_set_value(type, value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_kill_switch_get_value(type, value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        via_qmk_kill_switch_save(type);
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_kill_switch_get_value(uint8_t type, uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_kill_switch_enable:
      {
        value_data[0] = kill_switch_config[type].enable;
        break;
      }    
    case id_qmk_kill_switch_keycode_0:
      {
        value_data[0] = kill_switch_config[type].keycode[0] >> 8;
        value_data[1] = kill_switch_config[type].keycode[0] & 0xFF;        
        break;
      }
    case id_qmk_kill_switch_keycode_1:
      {
        value_data[0] = kill_switch_config[type].keycode[1] >> 8;
        value_data[1] = kill_switch_config[type].keycode[1] & 0xFF;        
        break;
      }
  }
}

void via_qmk_kill_switch_set_value(uint8_t type, uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_kill_switch_enable:
      {
        kill_switch_config[type].enable = value_data[0];
        break;
      }
    case id_qmk_kill_switch_keycode_0:
      {
        kill_switch_config[type].keycode[0] = value_data[0] << 8 | value_data[1];
        break;
      }
    case id_qmk_kill_switch_keycode_1:
      {
        kill_switch_config[type].keycode[1] = value_data[0] << 8 | value_data[1];
        break;
      }
  }
}

void via_qmk_kill_switch_save(uint8_t type)
{
  if (type == KILL_SWITCH_LR)
  {
    eeconfig_flush_kill_switch_lr(true);
  }  
  if (type == KILL_SWITCH_UD)
  {
    eeconfig_flush_kill_switch_ud(true);
  }  
}

#endif