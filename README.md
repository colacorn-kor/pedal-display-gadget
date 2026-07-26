# GUI/GG - 기타 페달보드 미니 디스플레이 가젯

ESP32-S3 기반 기타 페달보드용 디스플레이 플랫폼이다. 튜너, 사운드 시각화, dB Meter,
이미지, Bounce 앱과 출력 뮤트 제어를 제공하며 앱 레지스트리, 런처, 슬롯/NVS, 테마
시스템을 통해 기능을 확장한다. 기타 메인 출력은 소프트웨어를 통과하지 않는 아날로그
패스스루다.

## 현재 상태

- 하드웨어: ESP32-S3-DevKitC-1 N16R8 브레드보드 프로토타입
- 디스플레이: ST7796S 3.5인치 480x320 SPI
- 오디오 입력: PCM1808 I2S와 TL072 프론트엔드 조립 완료, 외부 9V 미연결로 동작 미검증
- 컨트롤러: TRS 6키 저항 래더 Basic 구현, Smart 컨트롤러는 Phase 2
- 미장착 하드웨어: SD 카드 모듈, 뮤트 회로
- PC 시뮬레이터: SDL2 창, 키보드 입력, 실오디오 캡처 지원

가장 최근 장치 상태와 다음 실기 절차는 [`LAB_STATE.md`](LAB_STATE.md), 미결 작업은
[`PUNCHLIST.md`](PUNCHLIST.md)를 확인한다. 에이전트 작업 규칙은 [`AGENTS.md`](AGENTS.md)에
있다.

## 아키텍처

```text
Core1 audio_task
  I2S RX
    +-- AUDIO_SPECTRUM -> fft_map -> 시각화 스냅샷 발행
    +-- AUDIO_TUNER    -> tuner   -> 튜너 결과 발행

Core0 display_task / LVGL
  발행된 복사본 -> 활성 gadget_app_t -> 화면 렌더

Core0 input_task
  TRS 래더 + FOOTSW -> ui_event_t -> 활성 앱 우선 디스패치

PC simulator
  platform_sim + 동일 앱/UI/renderer/tuner/music_events 코드
```

오디오/DSP 상태는 Core1이 단독 소유한다. UI는
`audio_viz_snapshot_get()`과 `tuner_get()`으로 일관된 복사본만 읽으며, 모든 LVGL 접근은
기존 lock 규약을 따른다.

## 등록 앱

| ID | 표시명 | 역할 |
|---|---|---|
| `monitor` | Sound Monitor | 로그 스펙트럼, 기타·베이스 12밴드, 원형 스펙트럼의 6개 프리셋 |
| `images` | Images | 내장 및 향후 SD 콘텐츠 표시 |
| `tuner` | Tuner | 진입 시 뮤트와 튜너 오디오 모드 소유 |
| `bounce` | Bounce | 소리 온셋으로 고양이가 종이컵을 넘는 러너 게임 |
| `dbmeter` | dB Meter | RMS/피크 dBFS와 ADC 핀 기준 Vrms·dBV·dBu 표시 |

앱은 `gadget_app_t` 계약과 `gadget_app.c` 레지스트리에 등록된다. 런처 항목은 레지스트리에서
생성되며 `app_slots.c`가 LIVE/STASH 슬롯과 NVS 설정을 관리한다.

런처는 `LIVE`, `STASH`, `Reorder/Settings`의 3개 행이다. 상·하는 빈 행을 포함해 행 사이를
이동하고 좌·우는 현재 행 안에서만 이동한다. 앱 화면에서 홈을 누르면
`Exit/Settings`, 런처의 Settings에는 `Theme/Info`가 열린다.

## 입력

Basic 컨트롤러는 G4 `TRS_SIG`의 ADC 비율을 window+deadzone으로 판정한다.

```text
Ring(+3V3) -- Rtop 10k -- Tip -- 220R -- G4
Tip -- key resistor -- Sleeve(GND)

UP=0R, DOWN=470R, LEFT=1k, RIGHT=2k, OK=4.7k, HOME=10k
```

최근접 판정은 동시 입력을 다른 키로 오인할 수 있어 사용하지 않는다. Ring은 +3V3 고정이며
5V를 연결하면 안 된다. 회로의 최종 권위는 `hardware/NETLIST_SPEC.md`, 조립 절차는
`ASSEMBLY.md`, Smart 확장 계약은 `CONTROLLER_DESIGN.md`다.

## 주요 파일

| 파일 | 역할 |
|---|---|
| `app_main.c` | ESP 부팅, Core0/Core1 태스크, I2S, 입력, UI queue |
| `gadget_app.{c,h}` | 앱 인터페이스와 레지스트리 |
| `app_monitor.c` | Sound Monitor 앱 |
| `app_images.c` | Images 앱 |
| `app_tuner.c` | Tuner 앱과 뮤트/오디오 모드 생명주기 |
| `app_bounce.c` | 온셋 기반 고양이 러너 Bounce 앱 |
| `app_db_meter.c` | dB Meter 앱 |
| `app_slots.{c,h}` | LIVE/STASH 슬롯과 NVS 영속성 |
| `screen_manager.c` | 런처, 활성 앱, 팝업, 앱 우선 이벤트 디스패치 |
| `renderer*.c`, `theme.c` | 로그·12밴드·원형 모니터 렌더러와 테마 |
| `audio_level.{c,h}` | dBFS·Vrms·dBV·dBu 변환 |
| `fft_map.{c,h}` | 2048-point FFT를 20Hz~20kHz, -72~0dBFS의 256점 로그 스펙트럼으로 매핑 |
| `tuner.{c,h}` | MPM/NSDF 피치 검출과 결과 발행 |
| `music_events.{c,h}` | 온셋, 피치, BPM 이벤트 |
| `display_bringup.{c,h}` | ST7796S와 esp_lvgl_port 초기화 |
| `platform_esp.c` | ESP 하드웨어 플랫폼 구현 |
| `sim/` | SDL2 PC 시뮬레이터와 `platform_sim` |
| `tests/` | MIDI, 튜너, 오디오 레벨 단위, FFT 기준 호스트 테스트 |

## 펌웨어 빌드

ESP-IDF v5.4.4 환경에서:

```powershell
idf.py set-target esp32s3
idf.py build
```

`sdkconfig.defaults`가 바뀌었거나 오래된 환경 캐시가 의심되면 파생 설정을 지우고 다시
구성한다.

```powershell
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
Remove-Item -Force sdkconfig,sdkconfig.old -ErrorAction SilentlyContinue
idf.py set-target esp32s3
idf.py build
```

`build/`, `managed_components/`, `sdkconfig`, `sdkconfig.old`는 Git 추적 대상이 아니다.

## 검증

호스트 테스트:

```powershell
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

PC 시뮬레이터의 Visual Studio/vcpkg 빌드와 실행법은
[`sim/README.md`](sim/README.md)를 따른다.

하드웨어 플래시 전에는 외부 9V를 분리하고 USB만 연결한다. 플래시 전체 삭제, eFuse,
보안 설정 변경은 일반 개발 절차에 포함하지 않는다.

## 확장 트랙

향후 코덱 오디오 출력, WiFi 업로더/OTA, UART/BLE MIDI, 스크립트 앱 로더,
Smart MCU 컨트롤러를 추가한다. 순서와 확정 계약은 `PROJECT_MASTER.md`와
`ARCHITECTURE.md`를 따른다.
