# GG - 모듈형 기타 사운드 디스플레이

ESP32-S3 기반 기타 페달보드용 디스플레이 플랫폼이다. 튜너, 사운드 시각화, dB Meter,
Oscilloscope, SD Gallery, Music/Game, MIDI Monitor와 출력 뮤트 제어를 제공하며 앱 레지스트리, 런처, 슬롯/NVS, 테마
시스템을 통해 기능을 확장한다. 기타 메인 출력은 소프트웨어를 통과하지 않는 아날로그
패스스루다. 제품 범위와 GG2의 경계는 [`GG_PRODUCT_SPEC.md`](GG_PRODUCT_SPEC.md)가
권위다.

## 현재 상태

- 하드웨어: ESP32-S3-DevKitC-1 N16R8 브레드보드 프로토타입
- 디스플레이: ST7796S 3.5인치 480x320 SPI
- 오디오 입력: PCM1808 I2S 연결, TL072+SPDT 분석 탭 재배선 필요
- 컨트롤러: TRS 6키 저항 래더 Basic 구현, Smart 컨트롤러는 Phase 2
- SD 카드: `SZH-EKBZ-005` 배선 완료, Gallery 실기 확인 대기
- 미장착 하드웨어: 뮤트 회로
- PC 시뮬레이터: SDL2 창, 키보드 입력, Windows 시스템 오디오·캡처 입력, WinMM MIDI, PC 폴더 저장소
- 개발 방향: 공통 앱·UI·설정을 PC에서 먼저 완성한 뒤 ESP 플랫폼 백엔드에 이식

가장 최근 장치 상태와 다음 실기 절차는 [`LAB_STATE.md`](LAB_STATE.md), 미결 작업은
[`PUNCHLIST.md`](PUNCHLIST.md), PC 기준 제품 계약은
[`PC_SIMULATOR_PRODUCT.md`](PC_SIMULATOR_PRODUCT.md)를 확인한다. 에이전트 작업 규칙은
[`AGENTS.md`](AGENTS.md)에 있다.

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
  platform_sim + 동일 앱/UI/renderer/fft_map/tuner/music_events 코드
```

오디오/DSP 상태는 Core1이 단독 소유한다. UI는
`audio_viz_snapshot_get()`과 `tuner_get()`으로 일관된 복사본만 읽으며, 모든 LVGL 접근은
기존 lock 규약을 따른다.

## 등록 앱

| ID | 표시명 | 역할 |
|---|---|---|
| `monitor` | Sound Monitor | Curve, 기타·베이스 12-Band, Circular, 무잔상 Reference, 4종 Weighting 표시 |
| `images` | Gallery | SD의 JPG/PNG/BMP/GIF/LVGL BIN 이미지 탐색·표시 |
| `tuner` | Tuner | 진입 시 뮤트와 튜너 오디오 모드 소유 |
| `dbmeter` | dB Meter | RMS/피크 dBFS와 ADC 핀 기준 Vrms·dBV·dBu 표시 |
| `music` | Music | PC WAV·내장 8비트 로비 트랙 재생, 현재 GG는 코덱 대기 |
| `game` | Game | 검증된 DMG `.gb` 타일 실행과 빈 타일의 내장 GG Cat 이스터 에그 |
| `metronome` | Metronome | 40~220 BPM, 2~5박자, 4종 분할과 PC click |
| `oscilloscope` | Oscilloscope | PCM 시간파형, trigger, timebase/scale, hold |
| `midimon` | MIDI Monitor | MIDI 메시지 이력·채널 필터·clock 집계 |

앱은 `gadget_app_t` 계약과 `gadget_app.c` 레지스트리에 등록된다. 런처 항목은 레지스트리에서
생성되며 `app_slots.c`가 LIVE/STASH 슬롯과 NVS 설정을 관리한다.

런처는 `LIVE`, `STASH`, `Reorder/Settings`의 3개 행이다. 상·하는 빈 행을 포함해 행 사이를
이동하고 좌·우는 현재 행 안에서만 이동한다. 앱 화면에서 홈을 누르면
`Settings/Info`, 런처의 Settings에는 `Theme/About`이 열린다. 앱의 즉시 나가기는
기존처럼 홈 길게 누르기가 담당한다.

런처의 `Theme → Mode`는 `Dark/Light`, `Theme → Color`는
`Blue/Green/Yellow/Red`를 선택하며 런처와 모든 공통 팝업에 함께 적용된다. 모든 앱의
`Settings → Theme → Color`는 `Default/Blue/Green/Yellow/Red` 중 해당 앱 콘텐츠 색만 바꾸고,
`Default`는 런처의 Mode와 Color를 모두 상속한다. 고정 앱 색도 전역 Dark/Light는
따른다. `Settings → Theme → Mode`는 색과 독립적으로 앱의 화면 형식을 선택한다.
Sound Monitor의 `Settings → Weighting`은 `Flat`, `A-weighted`, `Flat(Loudness)`,
`A-weighted(Loudness)`를 제공한다. 앱 화면에서는 상·하로 같은 네 항목을 바로 바꾸며,
끝 항목에서는 더 진행하지 않는다.

dB Meter 화면에서는 좌·우로 `INPUT`과 `WINDOW`를 선택하고 상·하로 선택한 값을 바꾼다.
같은 값은 `Settings → Input/Window`에서도 설정할 수 있다. 자동 듀얼레인지 빌드는 입력
범위를 하드웨어가 고르므로 수동 `Input` 항목을 표시하지 않는다.

## SD 카드

실기 모듈은 아두이노 MicroSD 카드 소켓 모듈 `SZH-EKBZ-005`다. VCC는 `+5V`,
GND는 공통 GND, MISO/MOSI/SCK/CS는 각각 G11/G13/G12/G47에 연결한다. 모듈의
온보드 3.3V LDO·레벨 변환을 사용하므로 VCC를 `+3V3`에 연결하지 않는다.

SD 카드는 FAT32로 포맷하고 다음 폴더를 사용한다.

```text
GG/
  images/   JPG, JPEG, PNG, BMP, GIF, LVGL BIN
  music/    WAV, MP3, FLAC, OGG (PC WAV 재생, GG 출력은 코덱 단계)
  games/    DMG .gb ROM과 자동 생성되는 같은 이름의 .sav
```

Gallery 진입 시 카드를 지연 마운트하므로 SD가 없어도 본체의 부팅과 다른 앱은 영향을 받지
않는다. 좌·우로 파일을 이동하고 OK로 카드와 폴더를 다시 검색하며, 재검색 뒤에도 같은 파일을
선택한다. 현재 한 폴더를 모두 확인한 뒤 대소문자를 무시한 자연 정렬(`Image2`가 `Image10`
보다 먼저)로 최대 64개를 표시하며 하위 폴더 재귀 탐색은 하지 않는다. 화면에는 형식·치수·
파일 크기가 표시되고, 정상 사진은 입력 없이 5초가 지나면 정보 배너를 숨겼다가 다음 버튼
입력에 다시 표시한다. 손상 파일은 별도 오류 화면으로 남는다. 이미지는 화면 크기인
480x320 이하를 권장한다. 더 큰 PNG/GIF도 파일 형식상 허용하지만 디코딩 메모리와 전환
시간이 크게 늘 수 있다. 카드 파일 시스템 손상을 막기 위해 카드를 빼거나 교체할 때는
본체 전원을 먼저 끈다.

Game은 소유·사용 권한이 있는 원본 Game Boy DMG `.gb`만 사용한다. 헤더 체크섬과 지원
카트리지 검사를 통과한 ROM만 타일에 나타나며, save RAM은 앱을 떠날 때 ROM 옆의 같은
이름 `.sav`에 기록된다. 방향키는 Game Boy D-pad이고, HOME 짧게로 화면 오른쪽의
`A/B/START/SELECT/BACK`을 고른 뒤 OK로 누른다. HOME 길게는 기존처럼 런처로 나가며
현재 외부 Game Boy 오디오는 PC와 GG 모두 무음이다.

## 입력

Basic 컨트롤러는 G4 `TRS_SIG`의 ADC 비율을 window+deadzone으로 판정한다.

```text
Ring(+3V3) -- Rtop 10k -- Tip -- 220R -- G4
Tip -- key resistor -- Sleeve(GND)

UP=0R, DOWN=470R, LEFT=1k, RIGHT=2k, OK=4.7k, HOME=10k
```

최근접 판정은 동시 입력을 다른 키로 오인할 수 있어 사용하지 않는다. Ring은 +3V3 고정이며
5V를 연결하면 안 된다. 현재 실물은 `hardware/AS_BUILT_WIRING.md`, 다음 권장 배선은
`ASSEMBLY.md`, 목표 회로는 `hardware/NETLIST_SPEC.md`, Smart 확장 계약은
`CONTROLLER_DESIGN.md`가 권위다.

## 주요 파일

| 파일 | 역할 |
|---|---|
| `app_main.c` | ESP 부팅, Core0/Core1 태스크, I2S, 입력, UI queue |
| `gadget_app.{c,h}` | 앱 인터페이스와 레지스트리 |
| `app_monitor.c` | Sound Monitor 앱 |
| `app_images.c` | SD Gallery 앱 |
| `app_tuner.c` | Tuner 앱과 뮤트/오디오 모드 생명주기 |
| `app_bounce.c`, `bounce_game.h` | Game 내부 GG Cat의 Dino식 런타임과 효과음·오디오 점프 |
| `app_db_meter.c` | dB Meter 앱 |
| `app_music.c`, `wav_decoder.{c,h}` | Music 앱과 공통 WAV 디코더 |
| `app_game.c` | Game 타일 로비·외부 플레이어와 빈 슬롯 이스터 에그 진입 |
| `game_runtime.{c,h}`, `game_core_peanut.c` | 공통 외부 게임 어댑터와 MIT Peanut-GB DMG 코어 |
| `app_metronome.c`, `metronome_engine.{c,h}` | Metronome UI와 sample-clock click 엔진 |
| `app_oscilloscope.c` | PCM 시간파형 Oscilloscope UI와 trigger/timebase/scale |
| `app_midi_monitor.c`, `midi_service.{c,h}` | MIDI Monitor와 공통 메시지 snapshot 서비스 |
| `audio_playback.{c,h}`, `audio_effects.{c,h}` | 공통 스테레오 transport·믹서와 앱 효과음 |
| `audio_autorange.{c,h}` | HOT/SENSITIVE 입력 스케일 환산, clip margin과 hysteresis 선택 |
| `app_slots.{c,h}` | LIVE/STASH 슬롯과 NVS 영속성 |
| `screen_manager.c` | 런처, 활성 앱, 팝업, 앱 우선 이벤트 디스패치 |
| `renderer*.c`, `theme.c` | 로그·12밴드·원형 모니터 렌더러와 테마 |
| `audio_level.{c,h}` | dBFS·Vrms·dBV·dBu 변환 |
| `fft_map.{c,h}` | 중심 시각을 맞춘 48k/12k/3kHz FFT를 20Hz~20kHz, -72~0dBFS 256점 로그 스펙트럼으로 매핑 |
| `spectrum_weighting.{c,h}` | 원 분석값과 분리된 Flat/A-weighted 및 60-phon Loudness 표시 응답 |
| `tuner.{c,h}` | MPM/NSDF 피치 검출과 결과 발행 |
| `music_events.{c,h}` | 온셋, 피치, BPM 이벤트 |
| `storage.{c,h}` | 이미지·음악·Game 콘텐츠 공통 카탈로그와 안전한 파일 접근 |
| `image_probe.{c,h}` | Gallery 이미지 형식·치수·끝 구조의 경량 사전 검사 |
| `storage_esp.c`, `sim/storage_sim.c` | SDSPI/FATFS와 PC 폴더 스토리지 백엔드 |
| `display_bringup.{c,h}` | ST7796S와 esp_lvgl_port 초기화 |
| `platform_esp.c` | ESP 하드웨어 플랫폼 구현 |
| `sim/` | SDL2 PC 시뮬레이터, `platform_sim`, PC용 FFT와 WinMM MIDI 백엔드 |
| `PC_SIMULATOR_PRODUCT.md` | PC 기준 제품과 ESP 이식 계약 |
| `tests/` | MIDI, 튜너, 오디오 레벨·weighting 단위, 공통 FFT 매핑 기준 호스트 테스트 |

## 펌웨어 빌드

ESP-IDF v5.4.4 환경에서:

```powershell
idf.py set-target esp32s3
idf.py build
```

GG는 16MB flash용 `partitions.csv`를 사용한다. 기존 장치와 같은 NVS·PHY·factory 시작
주소를 유지하면서 factory와 두 OTA 슬롯을 각각 4MB로 잡는다. 파티션 표를 처음 적용할
때는 `0x9000`의 24KB NVS를 먼저 백업하고, `erase-flash` 없이 일반 플래시한다.

기본 빌드는 현재 브레드보드의 PCM1808 VINL 단일채널을 사용한다. 목표 HOT와
SENSITIVE 회로를 모두 연결한 뒤에만 `-D AUDIO_DUAL_RANGE=1`로 스테레오 자동 범위 변형을
구성한다. 한 채널만 연결된 현재 장치에는 이 변형을 플래시하지 않는다. 듀얼 변형의
dB Meter `Range Diagnostics`는 두 ADC의 RMS/peak, 환산 입력 Vrms와 범위 간 mismatch를
외부 9V 단독 상태의 화면에서 확인하는 교정 도구다.

1kHz 실측 뒤에는 다음 CMake cache 변수로 보정값을 주입할 수 있다.

```text
-D AUDIO_DUAL_HOT_VOLTAGE_CORRECTION=1.0000
-D AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION=1.0000
-D AUDIO_GG_INPUT_FULL_SCALE_VPEAK=11.5000
```

값과 안전한 측정 순서는 `hardware/AUDIO_FRONTEND_ENGINEERING.md`를 따른다.

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
