cmake_minimum_required(VERSION 3.13)





# DEBOUNCE_TYPE 는 DEBOUNCE_RUNTIME 미사용 시의 fallback 이다.
set(DEBOUNCE_TYPE sym_defer_pk)

# 디바운스 타입/시간을 VIA 에서 런타임 전환 (기본값 GAMING / 20ms)
set(DEBOUNCE_RUNTIME ON)

# HOLD_ON_OTHER_KEY_PRESS 를 VIA 에서 런타임 on/off (기본값 ON)
set(HOLD_OKP_RUNTIME ON)

# TAPPING_TERM 을 VIA 에서 런타임 변경 (기본값 TAPPING_TERM 200ms, 범위 50~500ms)
set(TAPPING_TERM_RUNTIME ON)