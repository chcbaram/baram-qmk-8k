#ifndef KEYS_H_
#define KEYS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"



typedef struct
{
  uint8_t modifier;
  uint8_t reserved;
  uint8_t keycode[HW_KEYS_PRESS_MAX];
} keys_keycode_t;


bool keysInit(void);
void keysUpdate(void);
bool keysGetKeyCode(keys_keycode_t *p_keycode);

bool keysGetChangedCode(keys_keycode_t *p_keycode);
bool keysGetPressedCode(keys_keycode_t *p_keycode);

#ifdef __cplusplus
}
#endif

#endif