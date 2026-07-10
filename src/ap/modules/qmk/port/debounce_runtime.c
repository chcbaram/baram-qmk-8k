#include "quantum.h"

#ifdef DEBOUNCE_RUNTIME

#include "debounce.h"
#include "debounce_runtime.h"


// 심볼 리네임 래퍼(debounce_eager_pk.c / debounce_defer_pk.c)가 제공하는 함수들.
// 각각 stock 의 sym_eager_pk.c / sym_defer_pk.c 를 심볼명만 바꿔 컴파일한 것.
void debounce_eager_pk_init(uint8_t num_rows);
bool debounce_eager_pk(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, bool changed);
void debounce_eager_pk_free(void);

void debounce_defer_pk_init(uint8_t num_rows);
bool debounce_defer_pk(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, bool changed);
void debounce_defer_pk_free(void);


static uint8_t active_type     = DEBOUNCE_TYPE_GAMING;
static uint8_t g_debounce_time = 20;


void debounce_set_type(uint8_t type)
{
  active_type = (type == DEBOUNCE_TYPE_TYPING) ? DEBOUNCE_TYPE_TYPING : DEBOUNCE_TYPE_GAMING;
}

void debounce_set_time(uint8_t ms)
{
  g_debounce_time = ms;
}

uint8_t debounce_time_get(void)
{
  return g_debounce_time;
}


// 두 알고리즘 모두 초기화한다. 이후 스위칭 시 재init/free 없이 디스패치만 바꾼다.
void debounce_init(uint8_t num_rows)
{
  debounce_eager_pk_init(num_rows);
  debounce_defer_pk_init(num_rows);
}

void debounce_free(void)
{
  debounce_eager_pk_free();
  debounce_defer_pk_free();
}

bool debounce(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, bool changed)
{
  if (active_type == DEBOUNCE_TYPE_TYPING)
  {
    return debounce_defer_pk(raw, cooked, num_rows, changed);
  }
  return debounce_eager_pk(raw, cooked, num_rows, changed);
}

#endif

