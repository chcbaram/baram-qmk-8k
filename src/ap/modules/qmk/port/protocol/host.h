#pragma once

#include "hw_def.h"
#include "cli.h"

#include QMK_KEYMAP_CONFIG_H

#include "host_driver.h"
#include "qmk/quantum/led.h"


/* host driver */
void           host_set_driver(host_driver_t *driver);
host_driver_t *host_get_driver(void);


/* host driver interface */
uint8_t host_keyboard_leds(void);
led_t   host_keyboard_led_state(void);
void    host_keyboard_send(report_keyboard_t *report);
void    host_nkro_send(report_nkro_t *report);
void    host_mouse_send(report_mouse_t *report);
void    host_system_send(uint16_t usage);
void    host_consumer_send(uint16_t usage);
void    host_programmable_button_send(uint32_t data);

uint16_t host_last_system_usage(void);
uint16_t host_last_consumer_usage(void);