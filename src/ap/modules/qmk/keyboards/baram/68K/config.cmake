cmake_minimum_required(VERSION 3.13)



# RGB LIGHT
#
set(RGBLIGHT_ENABLE true)

add_compile_definitions(RGBLED_NUM=68)
add_compile_definitions(RGBLIGHT_EFFECT_RGB_TEST)
add_compile_definitions(RGBLIGHT_EFFECT_BREATHING)
add_compile_definitions(RGBLIGHT_EFFECT_RAINBOW_MOOD)
add_compile_definitions(RGBLIGHT_EFFECT_SNAKE)
add_compile_definitions(RGBLIGHT_EFFECT_STATIC_GRADIENT)


