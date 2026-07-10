// sym_defer_pk 를 심볼명만 바꿔 컴파일하는 래퍼.
// 실제 debounce* 심볼은 debounce_runtime.c 가 제공하므로 여기선 _defer_pk 접미사로 노출한다.
#ifdef DEBOUNCE_RUNTIME

#include "debounce_runtime.h"   // debounce_time_get 선언

#define debounce_init  debounce_defer_pk_init
#define debounce       debounce_defer_pk
#define debounce_free  debounce_defer_pk_free

#include "qmk/quantum/debounce/sym_defer_pk.c"

#endif
