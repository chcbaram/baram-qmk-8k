cmake_minimum_required(VERSION 3.13)
set(QMK_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/ap/modules/qmk")

set (QMK_KEYBOARD_PATH ${QMK_ROOT_PATH}/${KEYBOARD_PATH})

if (EXISTS "${QMK_KEYBOARD_PATH}/config.cmake")
  include ("${QMK_KEYBOARD_PATH}/config.cmake")
endif()

if (RGBLIGHT_ENABLE)
  list(APPEND QMK_ADD_FILES "${QMK_ROOT_PATH}/quantum/rgblight/rgblight.c")
  list(APPEND QMK_ADD_FILES "${QMK_ROOT_PATH}/quantum/process_keycode/process_rgb.c")
  list(APPEND QMK_ADD_FILES "${QMK_KEYBOARD_PATH}/driver/rgblight_drivers.c")
endif()  


# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB QMK_SRC_FILES CONFIGURE_DEPENDS
  ${QMK_ROOT_PATH}/*.c
  ${QMK_ROOT_PATH}/port/*.c
  ${QMK_ROOT_PATH}/port/platforms/*.c
  ${QMK_ROOT_PATH}/port/protocol/*.c

  ${QMK_ROOT_PATH}/quantum/quantum.c
  ${QMK_ROOT_PATH}/quantum/via.c
  ${QMK_ROOT_PATH}/quantum/keyboard.c
  ${QMK_ROOT_PATH}/quantum/action.c
  ${QMK_ROOT_PATH}/quantum/action_tapping.c
  ${QMK_ROOT_PATH}/quantum/action_util.c
  ${QMK_ROOT_PATH}/quantum/action_layer.c
  ${QMK_ROOT_PATH}/quantum/keycode_config.c
  ${QMK_ROOT_PATH}/quantum/led.c
  ${QMK_ROOT_PATH}/quantum/keymap_common.c
  ${QMK_ROOT_PATH}/quantum/eepconfig.c
  ${QMK_ROOT_PATH}/quantum/dynamic_keymap.c
  ${QMK_ROOT_PATH}/quantum/eeconfig.c
  ${QMK_ROOT_PATH}/quantum/keymap_introspection.c
  ${QMK_ROOT_PATH}/quantum/color.c


  ${QMK_ROOT_PATH}/quantum/sequencer/*.c
  ${QMK_ROOT_PATH}/quantum/logging/*.c
  # ${QMK_ROOT_PATH}/quantum/debounce/sym_eager_pk.c
  # ${QMK_ROOT_PATH}/quantum/debounce/asym_eager_defer_pk.c
  ${QMK_ROOT_PATH}/quantum/debounce/${DEBOUNCE_TYPE}.c
  

  ${QMK_ROOT_PATH}/quantum/send_string/*.c
  ${QMK_ROOT_PATH}/quantum/process_keycode/process_key_override.c
  ${QMK_ROOT_PATH}/quantum/process_keycode/process_grave_esc.c


  # ${QMK_KEYBOARD_PATH}/*.c
  ${QMK_KEYBOARD_PATH}/port/*.c
  
  ${QMK_ADD_FILES}
)

# 하위폴더에 있는 파일까지 포함한다.
#
# file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
# )


set (QMK_INC_DIR 
  ${QMK_ROOT_PATH}/port
  ${QMK_ROOT_PATH}/port/platforms 
  ${QMK_ROOT_PATH}/port/protocol
  ${QMK_ROOT_PATH}/quantum
  ${QMK_ROOT_PATH}/quantum/logging
  ${QMK_ROOT_PATH}/quantum/keymap_extras
  ${QMK_ROOT_PATH}/quantum/sequencer
  ${QMK_ROOT_PATH}/quantum/send_string
  ${QMK_ROOT_PATH}/quantum/process_keycode
  ${QMK_ROOT_PATH}/quantum/rgblight

  ${QMK_KEYBOARD_PATH}
)

add_compile_definitions(DYNAMIC_KEYMAP_MACRO_DELAY=10)

add_compile_definitions(VIA_ENABLE)
add_compile_definitions(RAW_ENABLE)
add_compile_definitions(DYNAMIC_KEYMAP_ENABLE)
add_compile_definitions(KEY_OVERRIDE_ENABLE)
add_compile_definitions(HOLD_ON_OTHER_KEY_PRESS)
add_compile_definitions(EXTRAKEY_ENABLE)


add_compile_definitions(QMK_KEYBOARD_H="quantum.h")
add_compile_definitions(QMK_KEYMAP_CONFIG_H="${QMK_KEYBOARD_PATH}/config.h")
add_compile_definitions(KEYMAP_C="${QMK_KEYBOARD_PATH}/keymap.c")




if (RGBLIGHT_ENABLE)
  add_compile_definitions(RGBLIGHT_ENABLE)
endif()