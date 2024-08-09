#pragma once

#include "quantum.h"



void kkuk_init(void);
void kkuk_idle(void);
bool kkuk_process(uint16_t keycode, keyrecord_t *record);
void via_qmk_kkuk_command(uint8_t *data, uint8_t length);