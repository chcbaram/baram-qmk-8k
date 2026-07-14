// VENOM 웹 대시보드용 raw HID 명령 핸들러.
//
// VIA 와 동일한 raw HID 채널(usage page 0xFF60)에 커스텀 명령 0xB0 을 추가한다.
// via_command_kb() 는 raw_hid_receive() 가 VIA 표준 명령을 처리하기 전에 먼저 호출되는
// weak 훅이며, 여기서 0xB0 을 가로채 응답 버퍼(data)를 in-place 로 채우고 true 를 반환한다.
// (USB 계층이 수정된 32바이트 버퍼를 그대로 호스트로 되돌려준다.)
//
// 제공 정보: 모델/펌웨어 버전/디바운스 설정/매트릭스 크기, 키 입력 레이턴시,
//            실시간 매트릭스 상태, 물리 레이아웃(펌웨어 임베드).

#include "quantum.h"
#include "usbd_hid.h"
#include "chattering.h"

#ifdef DEBOUNCE_RUNTIME
#include "debounce_runtime.h"
#endif


#define VENOM_RAW_CMD          0xB0

#define VENOM_SUB_INFO         0x01
#define VENOM_SUB_LATENCY      0x02
#define VENOM_SUB_MATRIX       0x03
#define VENOM_SUB_LAYOUT       0x04
#define VENOM_SUB_CHATTER      0x05
#define VENOM_SUB_USB_HEALTH   0x06

#define VENOM_API_VERSION      2   // 2: 점검(채터링/USB) 지원


// 보드별 물리 레이아웃(바이너리). 생성 파일 keyboards/<board>/port/layout_def.c 가 override 한다.
// 각 키 6바이트: x, y, w, h (1/4 키유닛), row, col.  레이아웃이 없으면 length 0.
__attribute__((weak)) uint16_t boardLayoutGet(const uint8_t **pp)
{
  *pp = NULL;
  return 0;
}


static void venom_cmd_info(uint8_t *data)
{
  const uint8_t *layout;
  uint16_t layout_len = boardLayoutGet(&layout);
  const char *ver = _DEF_FIRMWATRE_VERSION;

  data[2] = VENOM_API_VERSION;

#ifdef DEBOUNCE_RUNTIME
  data[3] = debounce_type_get();
  data[4] = debounce_time_get();
#else
  data[3] = 0xFF;   // 런타임 디바운스 미지원
  data[4] = 0;
#endif

  data[5] = MATRIX_ROWS;
  data[6] = MATRIX_COLS;
  data[7] = (uint8_t)(layout_len / 6);           // 키 개수
  data[8] = layout_len & 0xFF;                   // 레이아웃 총 바이트(LE)
  data[9] = (layout_len >> 8) & 0xFF;

  // 펌웨어 버전 문자열 (data[10..25], 최대 16바이트, null 패딩)
  for (int i = 0; i < 16; i++)
  {
    data[10 + i] = (ver[i] != 0) ? (uint8_t)ver[i] : 0;
    if (ver[i] == 0)
      break;
  }
}

static void venom_cmd_latency(uint8_t *data)
{
  uint16_t raw_us = 0, pre_us = 0, usb_us = 0;
  uint32_t seq = 0;
  bool has = usbHidGetLatency(&raw_us, &pre_us, &usb_us, &seq);

  data[2] = has ? 1 : 0;
  data[3] = (uint8_t)(seq & 0xFF);
  data[4] = raw_us & 0xFF;
  data[5] = (raw_us >> 8) & 0xFF;
  data[6] = pre_us & 0xFF;
  data[7] = (pre_us >> 8) & 0xFF;
  data[8] = usb_us & 0xFF;
  data[9] = (usb_us >> 8) & 0xFF;
}

static void venom_cmd_matrix(uint8_t *data)
{
  data[2] = MATRIX_ROWS;
  data[3] = MATRIX_COLS;

  uint8_t idx = 4;
  for (uint8_t row = 0; row < MATRIX_ROWS && idx + 1 < 32; row++)
  {
    matrix_row_t value = matrix_get_row(row);
    data[idx++] = value & 0xFF;
    data[idx++] = (value >> 8) & 0xFF;
  }
}

static void venom_cmd_layout(uint8_t *data)
{
  const uint8_t *layout;
  uint16_t layout_len = boardLayoutGet(&layout);
  uint16_t offset = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
  uint8_t  chunk = 0;

  // data[4] = 이번 청크 길이, data[5..] = 바이트 (최대 27)
  while (chunk < 27 && (offset + chunk) < layout_len)
  {
    data[5 + chunk] = layout[offset + chunk];
    chunk++;
  }
  data[4] = chunk;
}


// data = [ 0xB0, 0x05, action, arg_lo, arg_hi, ... ]
//   0 disable / 1 enable(window_ms=arg)+reset / 2 reset
//   3 read(start key index=arg): data[3]=엔트리수, data[4..]=키당4B [count, dur_lo, dur_hi, dbl]
//   4 detail(key index=arg): 단일키 상세 통계
static void venom_cmd_chatter(uint8_t *data)
{
  uint8_t  action = data[2];
  uint16_t arg    = (uint16_t)data[3] | ((uint16_t)data[4] << 8);

  chattering_notify_cmd();

  switch (action)
  {
    case 0:
      chattering_set_enable(false, 0);
      break;

    case 1:
      chattering_set_enable(true, arg);
      chattering_reset();
      break;

    case 2:
      chattering_reset();
      break;

    case 3:
    {
      uint16_t total = (uint16_t)MATRIX_ROWS * (uint16_t)MATRIX_COLS;
      uint8_t  idx   = 4;
      uint8_t  n     = 0;

      chattering_sweep(micros());

      while (n < 5 && (uint16_t)(arg + n) < total)   // 키당 5B[작동수_lo/hi, 에러율×10_lo/hi, 최대지연(0.25ms)], 5키(25B)
      {
        uint16_t acts = 0, over = 0, max_us = 0;
        uint32_t err10, q;
        chattering_get((uint16_t)(arg + n), &acts, &over, &max_us, NULL);

        err10 = acts ? ((uint32_t)over * 1000u + acts / 2u) / acts : 0;   // 에러율 ×10 (0.1%, 반올림 → 상세와 일치)
        if (err10 > 1000) err10 = 1000;
        q = max_us / 250u; if (q > 255) q = 255;            // 최대 지연 0.25ms 단위(그리드 표시용)

        data[idx++] = acts & 0xFF;                          // 작동수 2B (상세는 실제값, 키는 표시상 캡)
        data[idx++] = (acts >> 8) & 0xFF;
        data[idx++] = err10 & 0xFF;
        data[idx++] = (err10 >> 8) & 0xFF;
        data[idx++] = (uint8_t)q;
        n++;
      }
      data[3] = n;
      break;
    }

    case 4:
    {
      chatter_stat_t st;

      chattering_sweep(micros());
      if (chattering_get_detail(arg, &st))
      {
        uint16_t avg = st.chatter_count ? (uint16_t)(st.sum_dur_us / st.chatter_count) : 0;
        uint8_t  i   = 3;

        data[i++] = st.actuations & 0xFF;    data[i++] = (st.actuations >> 8) & 0xFF;
        data[i++] = st.chatter_count & 0xFF; data[i++] = (st.chatter_count >> 8) & 0xFF;
        data[i++] = st.over_count & 0xFF;    data[i++] = (st.over_count >> 8) & 0xFF;
        data[i++] = st.min_dur_us & 0xFF;    data[i++] = (st.min_dur_us >> 8) & 0xFF;
        data[i++] = st.max_dur_us & 0xFF;    data[i++] = (st.max_dur_us >> 8) & 0xFF;
        data[i++] = st.last_dur_us & 0xFF;   data[i++] = (st.last_dur_us >> 8) & 0xFF;
        data[i++] = avg & 0xFF;              data[i++] = (avg >> 8) & 0xFF;
        data[i++] = st.max_edges;
        data[i++] = st.last_edges;
      }
      break;
    }

    default:
      break;
  }
}

// data = [ 0xB0, 0x06, action ]  0 read / 1 reset
static void venom_cmd_usb_health(uint8_t *data)
{
  if (data[2] == 1)
  {
    usbHidResetLinkHealth();
    return;
  }

  usb_link_health_t h;
  uint8_t           i = 3;

  usbHidGetLinkHealth(&h);

  #define VENOM_PUT16(v)                                       \
    do {                                                       \
      uint32_t _v = (v);                                       \
      if (_v > 0xFFFF) _v = 0xFFFF;                            \
      data[i++] = (uint8_t)(_v & 0xFF);                        \
      data[i++] = (uint8_t)((_v >> 8) & 0xFF);                 \
    } while (0)

  VENOM_PUT16(h.reset_count);
  VENOM_PUT16(h.suspend_count);
  VENOM_PUT16(h.sof_stall_count);
  VENOM_PUT16(h.uptime_s);

  #undef VENOM_PUT16
}


bool via_command_kb(uint8_t *data, uint8_t length)
{
  (void)length;

  if (data[0] != VENOM_RAW_CMD)
  {
    return false;   // VIA 표준 명령은 그대로 통과
  }

  switch (data[1])
  {
    case VENOM_SUB_INFO:    venom_cmd_info(data);    break;
    case VENOM_SUB_LATENCY: venom_cmd_latency(data); break;
    case VENOM_SUB_MATRIX:  venom_cmd_matrix(data);  break;
    case VENOM_SUB_LAYOUT:  venom_cmd_layout(data);  break;
    case VENOM_SUB_CHATTER:    venom_cmd_chatter(data);    break;
    case VENOM_SUB_USB_HEALTH: venom_cmd_usb_health(data); break;
    default: break;
  }
  return true;
}
