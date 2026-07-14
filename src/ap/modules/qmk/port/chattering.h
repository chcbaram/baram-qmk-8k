#pragma once

#include "matrix.h"
#include <stdint.h>
#include <stdbool.h>


// 키별 채터링 상세 통계 (웹 클릭 상세용). 그리드 요약은 이 중 일부만 사용.
typedef struct
{
  uint16_t actuations;      // 누름(0->1) 작동 수
  uint16_t chatter_count;   // 바운스(윈도우 내 엣지 >= MIN_EDGES) 관측 횟수
  uint16_t over_count;      // 바운스 지속시간이 디바운스를 초과한(더블입력 유발 가능) 작동 수
  uint16_t min_dur_us;      // 바운스 지연시간 최소
  uint16_t max_dur_us;      // 바운스 지연시간 최대
  uint16_t last_dur_us;     // 바운스 지연시간 최근
  uint32_t sum_dur_us;      // 바운스 지연시간 합(평균 계산용)
  uint8_t  max_edges;       // 윈도우 내 최대 엣지 수
  uint8_t  last_edges;      // 윈도우 내 최근 엣지 수
} chatter_stat_t;


// 활성 플래그는 인라인 접근자로만 사용한다(소비자 코드가 변수를 직접 참조하지 않음).
// 아래 extern 은 인라인 구현 세부일 뿐이다.
extern volatile bool chattering_enabled_flag;

static inline bool chattering_is_enabled(void)
{
  return chattering_enabled_flag;
}


void chattering_set_enable(bool enable, uint16_t window_ms);
void chattering_reset(void);
void chattering_raw_scan(const matrix_row_t *curr, const matrix_row_t *prev_raw, uint32_t now_us);
void chattering_sweep(uint32_t now_us);
bool chattering_get(uint16_t key_index, uint16_t *actuations, uint16_t *over, uint16_t *max_us, uint16_t *chatter);
bool chattering_get_detail(uint16_t key_index, chatter_stat_t *out);
void chattering_task(void);
void chattering_notify_cmd(void);
