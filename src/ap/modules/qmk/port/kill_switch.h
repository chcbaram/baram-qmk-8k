#pragma once

#include "quantum.h"



void kill_switch_init(void);
bool kill_switch_process(uint16_t keycode, keyrecord_t *record);
bool kill_switch_is_use(uint16_t keycode);
void via_qmk_kill_swtich_command(uint8_t type, uint8_t *data, uint8_t length);