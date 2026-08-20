# GG PC 시뮬레이터 제품 계약

> 이 문서는 PC 시뮬레이터의 제품 역할, 본체와의 공통 코드 경계, 기능 이식 순서에 대한
> SSOT다. 실행·빌드 방법은 `sim/README.md`, 펌웨어 전체 구조는 `ARCHITECTURE.md`가 맡는다.

## 1. 제품 결정

PC 시뮬레이터는 화면을 잠깐 확인하는 모형이 아니라 다음 세 역할을 동시에 가진다.

1. GG의 앱·런처·설정을 먼저 완성하고 검수하는 **기준 구현**
2. 실제 GG가 없어도 오디오 분석·미디어·게임 앱을 사용할 수 있는 **독립 데스크톱 앱**
3. PC에서 검증된 공통 기능을 ESP32-S3 플랫폼 백엔드에 옮기는 **이식 원본**

새 앱과 사용자 기능은 특별한 이유가 없으면 PC 시뮬레이터에서 먼저 완성한다. 이후 ESP
작업은 같은 앱을 다시 만드는 일이 아니라 입력·출력·저장소 같은 하드웨어 백엔드를
연결하고 성능 한계를 조정하는 일이어야 한다.

## 2. 공통 코드와 플랫폼 코드

### 반드시 공통으로 사용하는 코드

- `gadget_app_t` 앱 구현과 레지스트리
- 런처, 슬롯, 공통 팝업, 전역 Dark/Light·Color와 앱 Color/Mode/세부 설정
- LVGL 화면과 렌더러
- FFT 매핑, 튜너, 레벨·BS.1770 loudness 계산, music event 검출
- 미디어 카탈로그, 재생 상태 기계, 게임 상태와 입력 규칙
- 설정 스키마와 마이그레이션 규칙

### 플랫폼별로 달라도 되는 코드

- 화면 출력: SDL 창 / ST7796 SPI
- 사용자 입력 수집: 키보드·게임패드 / TRS 6키·풋스위치
- 오디오 입력 수집: WASAPI loopback·마이크 / PCM1808 I2S
- 오디오 출력 전송: PC 오디오 장치 / 미래 GG 코덱 I2S
- 파일 저장소: PC 폴더 / FATFS MicroSD
- 설정 저장: PC 파일 / ESP NVS
- MIDI 전송: Windows WinMM / 미래 UART·BLE-MIDI
- 시간, 태스크, 파일 디코더의 플랫폼 최적화 부분

앱 파일 안에서 `PEDAL_SIM` 조건으로 별도 제품 동작을 만들지 않는다. 필요한 차이는
`platform_*`, `storage_*`, 향후 `audio_out_*`, `midi_transport_*`, `game_runtime_*`
경계 뒤에 둔다.

## 3. 플랫폼 대응표

| 기능 | PC | GG 본체 | 공통 계약 |
|---|---|---|---|
| 화면 | SDL2/LVGL 창 | ST7796/LVGL | 같은 480x320 UI 트리 |
| 입력 | 키보드, 향후 게임패드 | TRS 6키, 풋스위치 | 같은 `ui_event_t` |
| 분석 오디오 | Windows 시스템 출력 loopback 또는 캡처 장치 | PCM1808 I2S | 같은 48kHz 분석 스냅샷 |
| 오디오 출력 | PC 기본/선택 출력 장치 | 미래 헤드폰/AUX 코덱 | 같은 재생·믹서 API |
| 미디어 저장소 | `GG_SD_ROOT` 또는 `sim/sdcard` | FAT32 MicroSD | 같은 `storage_*`와 `GG/` 구조 |
| 설정 | 격리된 PC 파일 | NVS | 같은 설정 구조와 마이그레이션 |
| MIDI | WinMM 입출력·장치 열거 | unavailable stub, 향후 UART·BLE-MIDI | 같은 parser·서비스·앱 |
| 게임 콘텐츠 | PC `GG/games` 폴더 | MicroSD `GG/games` | 같은 카탈로그와 코어 어댑터 |

PC의 시스템 오디오 loopback은 PCM1808의 대체 입력이다. 현재 분석 코어가 단일 입력을
사용하므로 PC 스테레오는 분석용 mono로 내려간다. 미디어 재생의 스테레오 출력은 별도의
오디오 출력 경로로 보존한다.

## 4. 기능 능력치 계약

물리 부품 이름으로 앱을 막지 않고 플랫폼이 제공하는 능력으로 판단한다.

```text
DISPLAY
AUDIO_ANALYSIS_INPUT
AUDIO_PLAYBACK_OUTPUT
MEDIA_STORAGE
MIDI_INPUT / MIDI_OUTPUT
GAME_RUNTIME
```

`gadget_app_t.required_capabilities`와 플랫폼 능력 마스크로 가용성을 판단한다. PC에서는
SDL 출력 장치 개방에 성공하면 `AUDIO_PLAYBACK_OUTPUT`을 제공한다. Music은 이 능력이
필수라 출력이 없으면 비활성이고, Metronome·Game은 항상 실행하되 출력이 없으면
무음 시각 모드로 동작한다. 메인 기타 Thru는 이 능력에 포함하지 않으며 출력 경로 enum에
메인 출력 값을 만들지 않는 원칙도 유지한다.

## 5. 현재 기능 격차

| 기능 | PC 현재 | GG 현재 | 목표 |
|---|---|---|---|
| Launcher/Settings | 동작 | 동작 | UI와 세부 설정을 PC에서 먼저 개선 |
| Sound Monitor | 실오디오·합성 입력 동작 | 동작 | 공통 renderer 유지 |
| Tuner | 실오디오·합성 입력 동작 | 동작 | 공통 DSP 유지 |
| dB Meter | Level과 K-weighted M/S/I LUFS·4x true-peak 추정, 설정 동작 | 같은 공통 DSP, 실전압·계측 대조 대기 | GG는 교정값과 공인 미터 대조만 추가 |
| Oscilloscope | 시스템/캡처 PCM 시간파형·trigger·hold 동작 | 같은 Core1 snapshot 계약 | 실제 입력에서 trigger·부하 실기 검증 |
| Gallery | 상태 UI·자연 정렬·손상 검사·메타데이터·5초 배너 숨김 동작 | 같은 공통 코드, SD 실기 대기 | FAT32 카드 실기 검증 |
| Metronome | click·UI·설정 동작 | 무음 시각 모드 동작 | GG 코덱 백엔드에서 같은 click 출력 |
| Music | 내장 로비 음악·WAV 재생 동작 | 출력 코덱 부재로 비활성 | ESP 코덱 백엔드 이식 |
| Game | 내장 GG Cat과 검증된 DMG `.gb` 동작 | 같은 UI·코어, 외부 게임 오디오 무음 | 실제 SD·프레임·입력 실기 검증 |
| MIDI Monitor | WinMM Monitor 동작 | 공통 UI 동작, 물리 MIDI 백엔드 없음 | GG UART/BLE 전송계층 연결 |

카탈로그가 존재하는 것과 파일을 실제 재생·실행하는 것은 구분한다. UI에 실행 가능한 것처럼
보이게 만들지 않는다.

## 6. SD 콘텐츠 앱의 폴백 계약

- Gallery, Music, Game은 SD 콘텐츠 중심 앱이지만 저장소 능력 부재로 런처에서 막지 않는다.
- Gallery는 이미지가 없으면 어두운 `GG` 월페이퍼를 표시한다.
- Music은 파일이 없으면 내장 8비트 로비 트랙을 재생한다. 현재는 사용자가 제작할 원곡이
  준비되기 전까지 코드 생성 임시 트랙을 사용하며, 앱 종료 시 반드시 정지한다.
- Game은 정사각형 타일 로비를 표시한다. 플레이 가능한 외부 게임만 타일에 표시하며,
  빈 타일에서 OK를 누르면 이름을 노출하지 않은 내장 GG Cat을 시작한다.
- 매니저는 한 번에 앱 하나만 활성화하고 앱의 `on_exit`를 마친 뒤 다음 앱에 진입한다.
  재생 transport도 활성 앱 소유권을 따르므로 Music과 Game 오디오는 동시에 재생하지 않는다.

## 7. 구현 순서

### D0A. Sound Monitor 우선 최적화

- 실제 Windows 오디오와 합성 입력에서 Curve·12-Band·Circular·Reference의 프레임 시간,
  무음 복귀, 저역 해상도와 입력 응답을 회귀 검사한다.
- 공유 renderer·`fft_map`을 최적화하되 PC와 GG의 시각·시간 동작을 갈라놓지 않는다.
- 미디어·게임 구현보다 이 단계의 정확도와 안정성을 먼저 유지한다.
- [Voxengo SPAN](https://www.voxengo.com/product/span/)처럼 FFT block size, 시간 평균과
  spectrum smoothing을 서로 다른 개념으로 다룬다. 다만 GG는 정밀 분석 기본값이므로
  SPAN의 선택형 visual slope에 해당하는 `dB/oct` 보정은 두지 않고 Slope 0으로 고정한다.
- Reference는 3k/12k/48kHz의 2048-point FFT 창 중심 시각을 맞춰 혼합하고 시간 평균이나
  release를 적용하지 않는다. 고정 진폭 30/300/3000Hz와 250~700Hz 이동 sweep으로 위치,
  폭, 레벨 편차와 경계 이중 피크를 자동 검사한다.
- `Weighting`은 `Flat/A-weighted/Flat(Loudness)/A-weighted(Loudness)` 네 항목이다.
  Loudness는 1kHz 기준 60-phon 참조 곡선의 역감도 표시이고, A-weighted와 독립적으로
  합산할 수 있다. 원 분석값은 바꾸지 않으며 SPL 교정이 없는 PC loopback을 `dBA`, phon
  또는 절대 loudness 측정으로 표기하지 않는다. 상·하는 앱 화면에서 Weighting을 직접
  바꾸고, 표시 하한 6dB 안의 양의 보정은 무음 release 잔여값을 확대하지 않도록
  점진 적용한다.
- `--renderer-benchmark`는 12-Band와 Circular를 합성 sweep으로 구동해 redraw rate,
  평균·최대 renderer 시간, HOME 팝업 왕복 응답을 계측한다. 12-Band 10Hz, Circular 8Hz,
  최대 update 50ms, HOME 120ms를 최소 수용선으로 삼고 Circular의 20kHz→20Hz 방향과
  좌우 픽셀 대칭도 함께 검사한다.

### D0. 공통 UI 기준선

- 런처와 기존 5개 앱의 화면·입력·설정 불일치를 목록화한다.
- 데스크톱 창에서 모든 설정을 조작하고 NVS 대체 파일에 보존한다.
- 주요 화면 preview와 smoke test를 앱별 수용 기준으로 확장한다.
- dB Meter는 화면의 `좌우=Input/Window 선택`, `상하=값 변경`과
  `Settings → Input/Window`가 같은 상태를 조작하는지 smoke로 검사한다.

### D1. 플랫폼 능력과 PC 오디오 출력 *(완료)*

- 물리 `needs_codec` 판정을 앱 요구 capability와 플랫폼 능력 검사로 교체했다.
- 공통 48kHz 스테레오 transport는 활성 앱 ID 하나가 소유하고 Music/Effects 버스를
  gain 적용 후 믹스·클리핑한다. 디코더와 UI는 출력 장치를 직접 소유하지 않는다.
- PC는 SDL queued output을 쓰며 실제 기본/선택 장치 개방 성공 시에만
  `AUDIO_PLAYBACK_OUTPUT`을 제공한다. `--output-device N`과 `--list-audio`를 지원한다.
- 코덱 없는 ESP는 명시적 unavailable 상태를 반환하고 큐 메모리를 할당하지 않지만 같은
  공통 소스를 빌드한다. 자동 smoke는 무음 가상 sink에서 스테레오 경로를 검증한다.

### D2. Music *(완료)*

- `GG/music` 탐색, WAV 재생/일시정지, 이전/다음, 진행률, 볼륨과 오류 상태를 PC에서 완성했다.
- PCM 8/16/24/32-bit와 float32, mono/stereo, 8~192kHz WAV를 스트리밍으로 48kHz stereo로
  변환한다. 디코더는 앱 UI나 출력 장치를 소유하지 않는다.
- 파일이 없을 때 8초 길이의 코드 생성 임시 8비트 로비 트랙을 같은 transport와 믹서로
  반복 재생하고 앱 종료 시 소유권과 출력을 해제한다.
- MP3/FLAC/OGG는 카탈로그에 보이되 아직 지원하지 않는다는 오류를 표시한다. 메모리·라이선스·
  ESP 이식성을 확인한 뒤 같은 디코더 경계에 추가한다.

### D3. Game 타일 로비와 내장 게임 *(완료)*

- 사용자-facing 앱 이름은 Game으로 고정하고 가운데에 4개의 정사각형 슬롯을 둔다.
- 좌우로 슬롯을 고르고 빈 슬롯에서 OK를 누르면 이름을 표시하지 않는 내장 GG Cat으로
  진입한다. 빈 슬롯을 선택해도 이름·소스 메타데이터는 비워 둔다.
- GG Cat은 Chrome Dino식 대기·첫 점프 시작·가속·선인장·충돌·재시작 흐름을 사용한다.
  방향키/OK뿐 아니라 정규화 오디오 레벨이 0.35를 상향 통과할 때 점프하고, 0.25 아래에서
  다시 무장하는 히스테리시스로 지속음의 반복 점프를 막는다.
- 내장 게임의 HOME은 선택 위치를 유지한 채 로비로 돌아가고, 로비의 HOME은 표준 앱 메뉴를 연다.
- D3에서는 외부 파일을 숨기는 빈 타일 계약까지만 확정했다. D6에서 공통 코어 probe를
  통과한 DMG `.gb`를 빈 타일 앞의 플레이 가능한 타일로 승격했다.

### D4. Metronome과 앱 효과음 *(완료)*

- 48kHz sample-clock 엔진이 40~220 BPM, 2~5박자, quarter/eighth/triplet/sixteenth와
  첫 박 accent click을 생성한다.
- Metronome은 OK로 시작·정지하고 좌우로 Tempo/Meter/Division을 선택한 뒤 상하로 값을
  바꾼다. BPM·박자·분할은 앱 Settings와 지연 NVS 저장에도 연결했다.
- PC 출력에서는 click을 Effects 버스로 재생한다. Game의 내장 GG Cat도 같은 버스에서
  점프·장애물 통과·충돌 효과음을 내며, 자기 효과음이 loopback onset 점프를 만들지 않도록
  짧은 억제 구간을 둔다.
- 코덱 없는 GG에서는 같은 앱과 시각 상태를 유지하되 무음으로 동작한다. 미래 코덱 백엔드는
  공통 transport를 소비하므로 앱 코드를 다시 나누지 않는다.

### D5. Gallery와 미디어 UX *(완료)*

- Scanning/Loading/Ready/Empty/Error 상태와 빈 저장소의 어두운 `GG` 월페이퍼를 완성했다.
- PC 폴더와 SD가 같은 대소문자 무시 자연 정렬, 64개 표시 상한, 경로 초과·목록 초과
  집계와 오류 문구를 사용한다.
- BMP/PNG/JPEG/GIF/BIN의 기본 구조를 읽기 전에 검사하고, LVGL decoder 확인 뒤 형식·치수·
  파일 크기를 표시한다. 손상 파일은 앱을 멈추지 않고 해당 항목의 오류 화면으로 남는다.
- 긴 파일명은 원본 경로에 보존하고 하단 한 줄에서 말줄임한다. OK 재검색은 파일 경로로
  현재 선택을 복원한다.

### D6. 외부 게임 코어 *(완료)*

- MIT Peanut-GB를 고정 커밋으로 포함해 원본 Game Boy DMG `.gb`를 PC와 GG의 같은 코어
  어댑터로 실행한다. 160x144 프레임은 2배 정수 확대하고, 코어는 59.7Hz로 진행하면서
  30fps 화면을 발행한다.
- 공통 런타임은 코어 선택, ROM probe/load, 눌림 상태+이벤트 입력, 프레임 발행, 48kHz stereo
  audio sink 자리와 save RAM `.sav`를 소유한다. 첫 Peanut-GB 구성은 오디오를 생성하지 않는다.
- 확장자만 믿지 않고 선언 크기, 헤더 체크섬, DMG 호환 여부와 지원 카트리지 종류를 검사한
  파일만 로비에 표시한다. 실행 코어가 없는 NES/GBC 등은 카탈로그에 있어도 타일에 나오지 않는다.
- 6키 GG에서는 방향키가 D-pad, OK가 현재 A/B/START/SELECT/BACK 동작이며 HOME 짧게로
  동작을 순환한다. HOME 길게와 풋스위치의 전역 계약은 유지한다. PC는 Z/X/A/S 직접 키도 쓴다.
- 기존 1MiB 파티션을 바꾸지 않은 ESP 기본 빌드가 통과했다. GBA/NDS는 GG2 범위다.

### D7. MIDI Monitor *(완료)*

- parser 출력은 공통 MIDI 서비스에 발행한다. 서비스는 16개 비클록 메시지 이력,
  누적/clock 수, 최신 Program Change와 sequence를 lock-free snapshot으로 제공한다.
- MIDI Monitor는 현재 메시지와 5개 이력, 누적·clock·drop 수를 표시한다. 좌우 채널 필터,
  OK pause/live, UP clear를 제공하며 clock은 행을 채우지 않고 수만 센다.
- PC는 WinMM callback에서 짧은 메시지를 lock-free 큐에 넣고 시뮬레이터 메인 루프가 기존
  `midi_feed()`에 전달한다. GG의 UART/BLE 백엔드는 아직 unavailable stub이다.

### D8. ESP 이식과 실기

- PC에서 수용된 앱 코드는 바꾸지 않고 ESP 저장소·오디오 출력·MIDI 백엔드를 연결한다.
- PSRAM, 플래시, 프레임 시간, 디코딩 버퍼 차이는 플랫폼 설정과 제한값으로 다룬다.
- 깨끗한 ESP 빌드 뒤에만 플래시하고 화면·입력·오디오를 실기로 판정한다.

## 8. 공통 기능의 완료 조건

- PC 시뮬레이터에서 실제 사용자 흐름이 처음부터 끝까지 동작한다.
- 앱 UI와 상태 기계가 ESP 전용 파일이 아닌 공통 소스에 있다.
- 설정 저장·재시작 복원이 동작한다.
- 없는 장치는 능력 기반으로 명확히 비활성화되며 멈춤이나 가짜 성공이 없다.
- preview/smoke/호스트 테스트가 해당 기능의 정상·빈 상태·오류 상태를 포함한다.
- ESP 빌드는 PC 기능 추가와 동시에 깨지지 않는다.
- ESP 이식 때 앱을 다시 구현하지 않고 플랫폼 백엔드만 추가하거나 교체한다.

## 9. PC 독립 앱으로서의 기준

- GG 실물이 없어도 설치·실행하고 기본 앱을 사용할 수 있어야 한다.
- 시스템 오디오, 마이크/인터페이스, 미디어 폴더와 출력 장치를 사용자가 선택할 수 있어야 한다.
- 480x320 기준 UI는 유지하되 창 배율과 전체화면은 PC 표시 기능으로 제공한다.
- 명령행 옵션은 자동 테스트와 고급 설정용으로 남기고, 일상 기능은 앱 UI에서도 접근 가능하게 한다.
- PC 전용 편의 기능이 GG의 앱 모델과 설정 의미를 갈라놓지 않게 한다.
