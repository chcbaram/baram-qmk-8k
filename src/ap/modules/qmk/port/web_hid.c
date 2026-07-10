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

#ifdef DEBOUNCE_RUNTIME
#include "debounce_runtime.h"
#endif


#define VENOM_RAW_CMD          0xB0

#define VENOM_SUB_INFO         0x01
#define VENOM_SUB_LATENCY      0x02
#define VENOM_SUB_MATRIX       0x03
#define VENOM_SUB_LAYOUT       0x04

#define VENOM_API_VERSION      1


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
    default: break;
  }
  return true;
}
