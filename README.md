# baram-qmk-8k
- Custom keyboard firmware supporting 8Khz polling rate
- Key input processing is based on QMK firmware

## Specification
- ST STM32U5A5RJT6
- Polling rate 8Khz
- Scan rate 8Khz or higher
- VIA support (via JSON)

## Supported Keyboard
- BARAM
  - BARAM-45K
  - BARAM-53K
  - BARAM-68K
- GEON
  - VENOM-60MX-7U
  - VENOM-80MX-6.25U


## Block Diagram
- Architecture

![image1](https://github.com/chcbaram/baram-qmk-8k/assets/5537436/ed6a02b7-92d9-49c4-958b-8beb2d3d48de)

- Memory Map

![image2](https://github.com/chcbaram/baram-qmk-8k/assets/5537436/4f19f183-4d41-49b0-bcf0-57e988811bd5)


## Development environment
- CMake
- Make
- GCC ARM
  - Requires setting environment variables (Windows)
    - ARM_TOOLCHAIN_DIR
    - ex) D:/tools/gcc-arm-none-eabi-9-2020-q2-update-win32/bin
- Python


## How To Build
### CMake Configure (Mac/Linux)
```
cmake -S . -B build -DKEYBOARD_PATH='/keyboards/baram/68k'
```

### CMake Configure (Windows)
```
cmake -S . -B build -G "MinGW Makefiles" -DKEYBOARD_PATH='/keyboards/baram/68k'
```

### CMake Build
```
cmake --build build -j10
```


