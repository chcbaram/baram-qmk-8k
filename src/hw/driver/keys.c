#include "keys.h"


#ifdef _USE_HW_KEYS
#include "button.h"
#include "qbuffer.h"
#include "cli.h"
#include "key_scan/pssi.h"


typedef enum
{
  KEYS_NODE_IDLE,
  KEYS_NODE_PRESSED_CHECK,
  KEYS_NODE_RELEASED_CHECK,
  KEYS_NODE_PRESSED,
  KEYS_NODE_RELEASED,
} KeyscanNodeState_t;


typedef struct
{
  uint8_t  index;
  uint8_t  state;
  uint32_t time_exe; 
  bool     pin_cur;
  bool     pin_pre;
  uint8_t  pressed; 
} keys_event_q_t;


typedef struct
{
  KeyscanNodeState_t state_cur;
  KeyscanNodeState_t state_pre;

  uint32_t time_cur;
  uint32_t time_pre;  
  uint32_t time_exe; 
  bool     pin_cur;
  bool     pin_pre;
  uint8_t  pressed;
  bool     changed; 
} keys_node_t;

typedef struct
{
  uint32_t       key_cnt_max;
  uint32_t       key_update_cnt;
  keys_node_t node[HW_KEYS_MAX_CH];

  uint32_t key_cnt_pressed;
  uint16_t node_pressed[HW_KEYS_MAX_CH];

  uint32_t key_cnt_changed;
  uint16_t node_changed[HW_KEYS_MAX_CH];
} keys_info_t;


#if CLI_USE(HW_KEYS)
static void cliCmd(cli_args_t *args);
#endif


static keys_info_t    info;
static qbuffer_t         keys_event_q;
static keys_event_q_t keys_event_buf[100];




bool keysInit(void)
{
  qbufferCreateBySize(&keys_event_q, (uint8_t *)keys_event_buf, sizeof(keys_event_q_t), 100);

  info.key_cnt_max     = HW_KEYS_MAX_CH;
  info.key_update_cnt  = 0;
  info.key_cnt_changed = 0;
  info.key_cnt_pressed = 0;

  for (int i=0; i<info.key_cnt_max; i++)
  {
    info.node[i].state_cur = KEYS_NODE_IDLE;
    info.node[i].state_pre = KEYS_NODE_IDLE;
    info.node[i].pressed = false;
    info.node[i].changed = false;
  }


  pssiInit();

#if CLI_USE(HW_KEYS)
  cliAdd("keys", cliCmd);
#endif

  return true;
}

// From button.c
//
void buttonUpdateEvent(void)
{
  keysUpdate();
}

void keysUpdate(void)
{
  uint32_t time_cur;


  info.key_update_cnt++;
  time_cur = micros();

  info.key_cnt_pressed = 0;
  info.key_cnt_changed = 0;
  for (int i=0; i<info.key_cnt_max; i++)
  {
    bool is_changed = false;
    bool is_pressed = false;

    info.node[i].time_cur = time_cur;
    info.node[i].pin_pre = info.node[i].pin_cur;
    info.node[i].pin_cur = buttonGetPressed(i);

    switch(info.node[i].state_cur)
    {
      case KEYS_NODE_IDLE:
        info.node[i].pin_pre = info.node[i].pin_cur;
        info.node[i].time_pre = time_cur;
        if (info.node[i].pin_cur == true)
          info.node[i].state_cur = KEYS_NODE_PRESSED_CHECK;
        else
          info.node[i].state_cur = KEYS_NODE_RELEASED_CHECK;
        break;

      case KEYS_NODE_PRESSED_CHECK:
        break;

      case KEYS_NODE_RELEASED_CHECK:
        break;

      case KEYS_NODE_PRESSED:
        break;

      case KEYS_NODE_RELEASED:
        break;
    }
    info.node[i].state_pre = info.node[i].state_cur;
    info.node[i].time_exe = time_cur - info.node[i].time_pre;

    if (info.node[i].pin_cur == true)
    {
      is_pressed = true;
    }

    if (info.node[i].pin_pre != info.node[i].pin_cur)
    {
      is_changed = true;
      info.node[i].changed  = true;
      info.node[i].time_pre = time_cur;
      
    }

    if (is_changed)
    {
      keys_event_q_t event_q;

      event_q.index = i;
      event_q.pin_cur = info.node[i].pin_cur;
      event_q.pin_pre = info.node[i].pin_pre;
      event_q.time_exe = info.node[i].time_exe;

      qbufferWrite(&keys_event_q, (uint8_t *)&event_q, 1);

      info.node_changed[info.key_cnt_changed] = i;
      info.key_cnt_changed++;
    }

    if (is_pressed)
    {
      info.node_pressed[info.key_cnt_pressed] = i;
      info.key_cnt_pressed++;      
    }
  }
}

bool keysGetKeyCode(keys_keycode_t *p_keycode)
{
  p_keycode->reserved = 0;
  p_keycode->modifier = 0;

  return true;
}

#if CLI_USE(HW_KEYS)
void cliCmd(cli_args_t *args)
{
  bool ret = false;



  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("update cnt : %d\n", info.key_update_cnt);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "test"))
  {
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "event"))
  {
    uint32_t event_cnt;

    event_cnt = qbufferAvailable(&keys_event_q);
    cliPrintf("event_cnt : %d\n", event_cnt);

    for (int i=0; i<event_cnt; i++)
    {
      keys_event_q_t event_q;

      qbufferRead(&keys_event_q, (uint8_t *)&event_q, 1);

      cliPrintf("%d \n", i);
      cliPrintf("  index : %d \n", event_q.index);
      cliPrintf("  pin   : %d -> %d\n", event_q.pin_pre, event_q.pin_cur);
      cliPrintf("  time  : %d us, %d ms\n", event_q.time_exe, event_q.time_exe/1000);
    }

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("keys info\n");
    cliPrintf("keys test\n");
    cliPrintf("keys event\n");
  }
}
#endif

#endif