#include "chattering.h"
#include "keyboard.h"
#include "cli.h"
#include "usb.h"
#include "keys.h"
#include <string.h>

#ifdef DEBOUNCE_RUNTIME
#include "debounce_runtime.h"
#endif


#define CHATTER_WINDOW_MS_DEFAULT  40
#define CHATTER_MIN_EDGES          3
#define CHATTER_TIMEOUT_MS         3000u    // 웹 명령 끊김 시 자동 종료


// 현재 디바운스 시간(us). 바운스 지속시간이 이 값을 넘으면 디바운스가 못 걸러 더블입력 유발.
static uint32_t debounce_us(void)
{
#ifdef DEBOUNCE_RUNTIME
  return (uint32_t)debounce_time_get() * 1000u;
#else
  return 5000u;
#endif
}


volatile bool chattering_enabled_flag = false;

static uint32_t window_us_cfg = (uint32_t)CHATTER_WINDOW_MS_DEFAULT * 1000u;
static uint32_t last_cmd_ms   = 0;

// raw 윈도우 상태 (안정상태에서 첫 전이 시 윈도우를 열어 그 안의 엣지를 센다)
static uint8_t  win_active[MATRIX_ROWS][MATRIX_COLS];
static uint32_t win_start_us[MATRIX_ROWS][MATRIX_COLS];
static uint32_t win_last_us[MATRIX_ROWS][MATRIX_COLS];
static uint16_t win_edges[MATRIX_ROWS][MATRIX_COLS];

// 키별 누적 통계
static chatter_stat_t stat[MATRIX_ROWS][MATRIX_COLS];


static void chatter_finalize(uint8_t r, uint8_t c)
{
  if (!win_active[r][c])
    return;

  chatter_stat_t *s     = &stat[r][c];
  uint16_t        edges = win_edges[r][c];

  s->last_edges = (edges > 255) ? 255 : (uint8_t)edges;
  if (s->last_edges > s->max_edges)
    s->max_edges = s->last_edges;

  if (edges >= CHATTER_MIN_EDGES)
  {
    uint32_t dur   = win_last_us[r][c] - win_start_us[r][c];
    uint16_t dur16 = (dur > 0xFFFF) ? 0xFFFF : (uint16_t)dur;

    if (s->chatter_count < 0xFFFF)
      s->chatter_count++;
    s->last_dur_us = dur16;
    s->sum_dur_us += dur16;
    if (dur16 > s->max_dur_us)
      s->max_dur_us = dur16;
    if (s->min_dur_us == 0 || dur16 < s->min_dur_us)
      s->min_dur_us = dur16;

    // 바운스가 디바운스를 초과하면 더블입력 유발 가능 → 노후 판정용 카운트
    if (dur >= debounce_us() && s->over_count < 0xFFFF)
      s->over_count++;
  }

  win_active[r][c] = 0;
  win_edges[r][c]  = 0;
}


void chattering_raw_scan(const matrix_row_t *curr, const matrix_row_t *prev_raw, uint32_t now_us)
{
  uint32_t win = window_us_cfg;

  for (uint8_t r = 0; r < MATRIX_ROWS; r++)
  {
    matrix_row_t delta = curr[r] ^ prev_raw[r];

    while (delta)
    {
      uint8_t c = (uint8_t)__builtin_ctz(delta);
      delta &= delta - 1;

      if (win_active[r][c] && (uint32_t)(now_us - win_start_us[r][c]) <= win)
      {
        if (win_edges[r][c] < 0xFFFF)
          win_edges[r][c]++;
        win_last_us[r][c] = now_us;
      }
      else
      {
        // 만료된 윈도우가 있으면 확정
        if (win_active[r][c])
          chatter_finalize(r, c);

        // 윈도우는 '누름(0->1)' 엣지에서만 시작한다. 뗌은 윈도우를 열지 않아
        // 바운스율(바운스 관측/작동수)이 100% 를 넘지 않는다.
        if ((curr[r] >> c) & 1)
        {
          win_active[r][c]   = 1;
          win_start_us[r][c] = now_us;
          win_last_us[r][c]  = now_us;
          win_edges[r][c]    = 1;
          if (stat[r][c].actuations < 0xFFFF)
            stat[r][c].actuations++;   // 누름 즉시 카운트(표시 지연 없음)
        }
        else
        {
          win_active[r][c] = 0;   // 뗌 엣지(활성 윈도우 없음)는 무시
        }
      }
    }
  }
}


void chattering_sweep(uint32_t now_us)
{
  uint32_t win = window_us_cfg;

  for (uint8_t r = 0; r < MATRIX_ROWS; r++)
  {
    for (uint8_t c = 0; c < MATRIX_COLS; c++)
    {
      if (win_active[r][c] && (uint32_t)(now_us - win_start_us[r][c]) > win)
        chatter_finalize(r, c);
    }
  }
}


void chattering_set_enable(bool enable, uint16_t window_ms)
{
  if (enable)
  {
    if (window_ms == 0)
      window_ms = CHATTER_WINDOW_MS_DEFAULT;
    window_us_cfg = (uint32_t)window_ms * 1000u;
    last_cmd_ms   = millis();
    chattering_enabled_flag = true;
  }
  else
  {
    chattering_enabled_flag = false;
    memset(win_active, 0, sizeof(win_active));   // 열린 윈도우 무효화
  }
}


void chattering_reset(void)
{
  memset(stat,       0, sizeof(stat));
  memset(win_active, 0, sizeof(win_active));
  memset(win_edges,  0, sizeof(win_edges));
}


bool chattering_get(uint16_t key_index, uint16_t *actuations, uint16_t *over, uint16_t *max_us, uint16_t *chatter)
{
  uint8_t r = (uint8_t)(key_index / MATRIX_COLS);
  uint8_t c = (uint8_t)(key_index % MATRIX_COLS);

  if (r >= MATRIX_ROWS)
    return false;

  if (actuations) *actuations = stat[r][c].actuations;
  if (over)       *over       = stat[r][c].over_count;
  if (max_us)     *max_us     = stat[r][c].max_dur_us;
  if (chatter)    *chatter    = stat[r][c].chatter_count;
  return true;
}


bool chattering_get_detail(uint16_t key_index, chatter_stat_t *out)
{
  uint8_t r = (uint8_t)(key_index / MATRIX_COLS);
  uint8_t c = (uint8_t)(key_index % MATRIX_COLS);

  if (r >= MATRIX_ROWS || out == NULL)
    return false;

  *out = stat[r][c];
  return true;
}


void chattering_task(void)
{
  if (chattering_enabled_flag && (millis() - last_cmd_ms) > CHATTER_TIMEOUT_MS)
  {
    chattering_set_enable(false, 0);
    logPrintf("[  ] chattering auto-off (timeout)\n");
  }
}


void chattering_notify_cmd(void)
{
  last_cmd_ms = millis();
}
