// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H




const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {
      // 0        1        2        3        4        5        6        7        8       9        10       11       12       13       14       15
        {KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,   KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSLS, KC_BSPC, KC_BSPC},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,   KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, _______, _______},
        {KC_CAPS, KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,   KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_ENT,  _______, _______},
        {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,   KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_RSFT, _______, _______, _______},
        {KC_LCTL, KC_LGUI, KC_LALT, _______, _______, _______, KC_SPC, _______, _______, _______, _______, KC_RALT, MO(1),   KC_RCTL, _______, _______}
    }         
};

void usbHidSetStatusLed(uint8_t led_bits)
{
  if (led_bits & USB_HID_LED_CAPS_LOCK)
    ws2812SetColor(0, WS2812_COLOR_GREEN);
  else
    ws2812SetColor(0, WS2812_COLOR_OFF);

  ws2812Refresh();
}