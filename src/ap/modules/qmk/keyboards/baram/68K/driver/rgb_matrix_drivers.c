#include "quantum.h"




// LED color buffer
rgb_led_t rgb_matrix_ws2812_array[WS2812_MAX_CH];
bool      ws2812_dirty = false;

static void init(void)
{
  ws2812_dirty = false;
}

static void flush(void)
{
  if (ws2812_dirty)
  {
    ws2812Refresh();
    ws2812_dirty = false;
  }
}

// Set an led in the buffer to a color
static inline void setled(int i, uint8_t r, uint8_t g, uint8_t b)
{
  if (rgb_matrix_ws2812_array[i].r == r && rgb_matrix_ws2812_array[i].g == g && rgb_matrix_ws2812_array[i].b == b)
  {
    return;
  }

  ws2812_dirty                 = true;
  rgb_matrix_ws2812_array[i].r = r;
  rgb_matrix_ws2812_array[i].g = g;
  rgb_matrix_ws2812_array[i].b = b;

  ws2812SetColor(i, WS2812_COLOR(r, g, b));
}

static void setled_all(uint8_t r, uint8_t g, uint8_t b)
{
  for (int i = 0; i < WS2812_MAX_CH; i++)
  {
    setled(i, r, g, b);
  }
}

const rgb_matrix_driver_t rgb_matrix_driver = {
  .init          = init,
  .flush         = flush,
  .set_color     = setled,
  .set_color_all = setled_all,
};

// led_config_t g_led_config = {
//     {
//         { 13, 12, 11,     10,      9,  8,      7,      6,  5,  4,  3,  2,      1,  0 },
//         { 27, 26, 25,     24,     23, 22,     21,     20, 19, 18, 17, 16,     15, 14 },
//         { 40, 39, 38,     37,     36, 35,     34,     33, 32, 31, 30, 29, NO_LED, 28 },
//         { 53, 52, 51,     50,     49, 48,     47,     46, 45, 44, 43, 42, NO_LED, 41 },
//         { 62, 61, 60, NO_LED, NO_LED, 59, NO_LED, NO_LED, 58, 57, 56, 55, NO_LED, 54 }
//     }, 
//     {
//         { 216,   0 }, { 192,   0 }, { 176,   0 }, { 160,   0 }, { 144,   0 }, { 128,   0 }, { 112,   0 }, {  96,   0 }, {  80,   0 }, {  64,   0 }, {  48,   0 }, {  32,   0 }, {  16,   0 }, {   0,   0 },
//         { 220,  16 }, { 200,  16 }, { 184,  16 }, { 168,  16 }, { 152,  16 }, { 136,  16 }, { 120,  16 }, { 104,  16 }, {  88,  16 }, {  72,  16 }, {  56,  16 }, {  40,  16 }, {  24,  16 }, {   4,  16 },
//         { 214,  32 }, { 188,  32 }, { 172,  32 }, { 156,  32 }, { 140,  32 }, { 124,  32 }, { 108,  32 }, {  92,  32 }, {  76,  32 }, {  60,  32 }, {  44,  32 }, {  28,  32 },               {   6,  32 },
//         { 224,  48 },               { 208,  48 }, { 186,  48 }, { 164,  48 }, { 148,  48 }, { 132,  48 }, { 116,  48 }, { 100,  48 }, {  84,  48 }, {  68,  48 }, {  52,  48 }, {  36,  48 }, {   9,  48 },
//         { 224,  64 }, { 208,  64 }, { 192,  64 },                                           { 176,  64 },                             { 160,  64 }, { 102,  64 }, {  42,  64 }, {  22,  64 }, {   2,  64 }
//     }, 
//     {
//         1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1,
//         4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1,
//         1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    1,
//         1,    1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1,
//         1, 1, 1,          1,       1, 4, 1, 1, 1
//     }
// };