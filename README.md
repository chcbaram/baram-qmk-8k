# baram-qmk-8k
- Custom keyboard firmware supporting 8Khz polling rate
- Key input processing is based on QMK firmware


## Development environment
- CMake
- Make
- GCC ARM
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


