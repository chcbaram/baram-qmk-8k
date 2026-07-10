#pragma once

#include <stdint.h>


enum debounce_type
{
  DEBOUNCE_TYPE_GAMING = 0,   // sym_eager_pk : 즉시 반영
  DEBOUNCE_TYPE_TYPING = 1,   // sym_defer_pk : 안정화 후 반영
};


// 런타임 디바운스 알고리즘/시간 선택
void    debounce_set_type(uint8_t type);
void    debounce_set_time(uint8_t ms);
uint8_t debounce_time_get(void);   // stock 디바운스 파일이 카운터 로드에 사용
