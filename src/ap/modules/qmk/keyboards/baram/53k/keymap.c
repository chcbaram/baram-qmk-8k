// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H




const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {
      // 0        1        2        3        4        5        6        7      
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    _______ }, // 0
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    _______, _______ }, // 1
        {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    _______, _______ }, // 2
        {KC_LCTL, KC_LGUI, KC_LALT, KC_SPC,  _______, _______, KC_RALT, _______ }, // 3

        {KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_DEL }, // 0 
        {KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_ENT,  KC_PGUP}, // 2
        {KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_UP,   KC_PGDN}, // 3
        {_______, _______, _______, MO(1),   KC_RCTL, KC_LEFT, KC_DOWN, KC_RGHT}  // 4
    }         
};
