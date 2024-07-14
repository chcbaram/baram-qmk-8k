#include "led_port.h"
#include "color.h"
#include "eeconfig.h"


#define LED_TYPE_MAX_CH       2

enum
{
  LED_TYPE_CAPS = 0,
  LED_TYPE_SCROLL
};

typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t enable : 2;
    uint8_t mode   : 6;
    HSV     hsv;
  };
} led_config_t;

_Static_assert(sizeof(led_config_t) == sizeof(uint32_t), "EECONFIG out of spec.");

enum via_qmk_led_value {
    id_qmk_led_enable       = 1,
    id_qmk_led_brightness   = 2,
    id_qmk_led_color        = 3,
};


static void via_qmk_led_get_value(uint8_t led_type, uint8_t *data);
static void via_qmk_led_set_value(uint8_t led_type, uint8_t *data);
static void via_qmk_led_save(uint8_t led_type);

static led_t leds = {0};
static led_config_t led_config[LED_TYPE_MAX_CH];


EECONFIG_DEBOUNCE_HELPER(led_caps,   EECONFIG_USER_LED_CAPS,   led_config[LED_TYPE_CAPS]);





void led_init_ports(void)
{
  eeconfig_init_led_caps();
  if (led_config[LED_TYPE_CAPS].mode != 1)
  {
    led_config[LED_TYPE_CAPS].mode = 1;
    led_config[LED_TYPE_CAPS].enable = true;
    led_config[LED_TYPE_CAPS].hsv    = (HSV){HSV_GREEN};
    eeconfig_flush_led_caps(true);
  }
}

void led_update_ports(led_t led_state)
{
  uint32_t led_color;
  RGB      rgb_color;


  led_color = WS2812_COLOR_OFF;
  if (led_config[LED_TYPE_CAPS].enable && led_state.caps_lock)
  {
    rgb_color = hsv_to_rgb(led_config[LED_TYPE_CAPS].hsv);
    led_color = WS2812_COLOR(rgb_color.r, rgb_color.g, rgb_color.b);
  }
  ws2812SetColor(0, led_color);
  

  ws2812Refresh();  
}

uint8_t host_keyboard_leds(void)
{
  return leds.raw;
}

void usbHidSetStatusLed(uint8_t led_bits)
{
  leds.raw = led_bits;  
}

void via_qmk_led_command(uint8_t led_type, uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_led_set_value(led_type, value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_led_get_value(led_type, value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        via_qmk_led_save(led_type);
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_led_get_value(uint8_t led_type, uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);
  switch (*value_id)
  {
    case id_qmk_led_enable:
      {
        value_data[0] = led_config[led_type].enable;
        break;
      }    
    case id_qmk_led_brightness:
      {
        value_data[0] = led_config[led_type].hsv.v;
        break;
      }
    case id_qmk_led_color:
      {
        value_data[0] = led_config[led_type].hsv.h;
        value_data[1] = led_config[led_type].hsv.s;
        break;
      }
  }
}

void via_qmk_led_set_value(uint8_t led_type, uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);
  switch (*value_id)
  {
    case id_qmk_led_enable:
      {
        led_config[led_type].enable = value_data[0];
        break;
      }
    case id_qmk_led_brightness:
      {
        led_config[led_type].hsv.v = value_data[0];
        break;
      }
    case id_qmk_led_color:
      {
        led_config[led_type].hsv.h = value_data[0];
        led_config[led_type].hsv.s = value_data[1];
        break;
      }
  }

  led_update_ports(leds);
}

void via_qmk_led_save(uint8_t led_type)
{
  if (led_type == LED_TYPE_CAPS)
  {
    eeconfig_flush_led_caps(true);
  }  
}
