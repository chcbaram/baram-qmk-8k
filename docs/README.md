# VENOM MX Keyboard Manager (WebHID)

VENOM 키보드의 **키 입력 레이턴시 측정**과 **매트릭스(키) 테스트**를 브라우저에서
바로 할 수 있는 웹 대시보드입니다. VIA와 동일하게 raw HID(usage page `0xFF60`)로
연결하며, 설치가 필요 없습니다.

## 기능
- **모델 / 펌웨어 버전 / 디바운스 타입·시간** 표시 (펌웨어에서 직접 읽음)
- **레이턴시**: 키 누름(첫 raw 접점) → USB 실제 전송 완료까지 측정, **디바운스 대기 포함** (ms 표시)
  - 디바운스 타입(GAMING/TYPING)·시간에 따른 실제 입력 지연을 비교 가능
  - 현재/평균/최소/최대 + 분포 히스토그램, 펌웨어 처리·USB 구간 분해 표시
  - (내부 전송은 µs, 표시는 ms)
- **키 테스터**: 물리 키 배열을 그려 눌린 키/테스트된 키 하이라이트
  - 키 배열(레이아웃)은 펌웨어에 임베드되어 raw HID 로 전송되므로, 웹은 모델별 JSON이 필요 없음
- **VENOM 모델만** 연결 목록에 표시 (VID 0x0483 + 지원 PID)

## 요구사항
- **Chrome / Edge (데스크톱)** — WebHID 지원 브라우저 (VIA와 동일 제약, Firefox/Safari 불가)
- HTTPS 또는 localhost (GitHub Pages 는 HTTPS 제공)

## 사용
1. 페이지 접속 → **연결** 클릭 → 목록에서 VENOM 선택
2. **레이턴시** 탭: 키를 여러 번 눌러 샘플 수집
3. **키 테스터** 탭: 키를 눌러 매트릭스 동작 확인

## GitHub Pages 배포
저장소 **Settings → Pages → Build and deployment**:
- Source: `Deploy from a branch`
- Branch: `main` / 폴더 `/docs`

배포 후 주소: `https://<사용자>.github.io/baram-qmk-8k/`

## 펌웨어 프로토콜 (raw HID, 32바이트)
요청/응답 모두 32바이트. 명령 = `data[0]=0xB0`, 서브 = `data[1]`.

| 서브 | 의미 | 응답 주요 필드 |
|---|---|---|
| `0x01` | 정보 | `[3]`type `[4]`time `[5]`rows `[6]`cols `[7]`keyCnt `[8..9]`layoutLen `[10..25]`ver |
| `0x02` | 레이턴시 | `[2]`valid `[3]`seq `[4..5]`raw `[6..7]`pre `[8..9]`usb (µs, LE) |
| `0x03` | 매트릭스 | `[2]`rows `[3]`cols `[4..]`row별 uint16 LE |
| `0x04` | 레이아웃 | 요청 `[2..3]`offset → 응답 `[4]`len `[5..]`bytes (키당 x,y,w,h,row,col) |

구현: 펌웨어 `src/ap/modules/qmk/port/web_hid.c` (`via_command_kb` 훅).
레이아웃 생성: `tools/gen_layout.py` (빌드 시 CMake가 현재 보드에 대해 자동 생성).
