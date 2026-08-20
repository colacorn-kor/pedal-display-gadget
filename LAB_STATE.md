# LAB_STATE.md - 현재 장치 및 실기 상태

> 갱신일: 2026-08-20
> 이 문서는 마지막 플래시와 현재 실험 상태의 SSOT다.

## 1. 기준 상태

- 플래시 이미지 기준 커밋: `7e6b8199-dirty` (`Optimize 12-band and circular renderers` 이후 개발본)
- 마지막 실기 펌웨어: 런처 오버플로 표시·Oscilloscope·MIDI Monitor와 16MB 파티션 표를
  포함한 2026-08-07 개발본
- 현재 펌웨어 상태: COM4 전체 삭제 없이 플래시, 16MB 파티션·8MB PSRAM 확인과 30초
  부팅 로그 통과. 이어 45초 입력 로그에서 오류·watchdog 없이 OK 입력을 확인했다.
- 현재 소스 개발본(미플래시): 독립 Bounce 앱을 제거해 등록 앱은 9개다. Game 빈 슬롯은
  이름과 `built-in` 정보를 전혀 표시하지 않고, OK로 고양이 캐릭터의 Chrome Dino식 내장
  GG Cat을 실행한다. 버튼과 임계 레벨 이상의 오디오 입력으로 점프한다. 일반 런처 앱
  타일의 우하단 커서는 제거하고 Reorder 커서만 유지했으며 STASH 행을 12px 올렸다.
  Gallery는 정상 사진에서 5초 무입력 뒤 정보 배너를 숨기고 입력 시 복귀한다. Sound
  Monitor는 여섯 대역명을 표시하고 Bass 색을 Sub Bass에 가깝게 조정했다. 깨끗한 PC
  빌드와 전체 smoke, 호스트 C 테스트 15개와 FFT 정규화 Python 테스트, ESP-IDF 기본 및
  `INPUT_TRS_LADDER=0` 빌드가 모두 통과했다. 기본 펌웨어는 `0xfea90` bytes이고 4MB 앱
  파티션의 75%가 남는다. 본체 플래시는 아직 하지 않았다.
- 2026-08-20 현재 소스 개발본(미플래시): 튜너에 무음 gate, 적응 평활과 2프레임 음이름
  전환 확인을 추가했다. dB Meter에는 전용 `AUDIO_METER` 경로와 K-weighted Momentary/
  Short-term/Integrated LUFS, 4배 true-peak 추정 및 OK 리셋을 추가했다. Sound Monitor의
  Curve/Reference는 OK로 현재 스펙트럼 비교 기준선을 고정·해제한다. PC 전체 smoke와
  호스트 17/17, 깨끗한 ESP-IDF 기본 및 `INPUT_TRS_LADDER=0` `-Werror=all` 빌드가
  통과했다. 이미지는 각각 `0x102cb0`/`0xff600`이고 4MB 앱 슬롯의 75%가 남는다.
- 마지막 플래시 개발본: Sound Monitor 3단계 FFT·4종 Weighting·6개 주파수 구간과 공통
  Theme/Settings, dB Meter Input/Window 조작을 포함한다. 이어 플랫폼 capability와 공통
  재생 transport, PC Music/WAV, Game 타일 로비·내장 Cat Run, Metronome과 게임 효과음,
  Oscilloscope와 MIDI Monitor까지 구현해 당시 등록 앱은 10개다. Setlist는 제품 범위에서
  제거했다. PC·호스트·ESP 자동 검증은 통과했다. 16MB 커스텀 파티션 표를 본체에 적용해
  factory와 OTA 슬롯을 각각 4MB로 예약했다. 런처 화살표·Oscilloscope 화면과 조작,
  실제 오디오 입력과 헤드폰 출력은 사용자 육안·청감 확인을 기다린다.
- 마지막 확인 포트: COM4
- 전원 전제: 사용자가 별도로 알리지 않는 한 외부 9V는 분리, USB만 연결된 상태
- 마지막 실기 플래시본 등록 앱: Sound Monitor, Gallery, Tuner, Bounce, dB Meter, Music,
  Game, Metronome, Oscilloscope, MIDI Monitor 총 10개
- 조립 상태: 디스플레이·PCM1808·TRS 6키·풋스위치 연결, SD 배선 완료
- 미장착: 재생/AUX/헤드폰 모듈, MIDI 물리 회로, 뮤트 회로
- 보유 SD 모듈: 아두이노 MicroSD 카드 소켓 모듈 `SZH-EKBZ-005`
  (VCC 4.5~5.5V, 온보드 3.3V LDO·레벨 변환), 배선 완료·기능 확인 대기
- Ring 100Ω / Tip 220Ω: 현재 장착됨
- 오디오 입력 프론트엔드: 현재 끝점 재확인 필요, 외부 9V 동작 미검증. 임시 TL072+SPDT를
  다시 만들지 않고 OPA2192 자동 듀얼레인지로 진행
- 목표 자동 듀얼레인지 회로는 문서와 소프트웨어만 준비됐으며 OPA2192를 보유하지 않음
- 목표 주변 회로: TLV320DAC3100 재생/AUX/헤드폰, 6N138+SN74AHCT14 MIDI,
  G39+AQY221R2S 병렬 뮤트, D1 1N5822+LM66200 전원 보호로 확정. 부품 미구매·미배선,
  ESP codec/MIDI backend 미구현
- 현재 실물 배선은 `hardware/AS_BUILT_WIRING.md`, 다음 권장 배선은 `ASSEMBLY.md`,
  미래 회로와 교정은 `hardware/NETLIST_SPEC.md`와
  `hardware/AUDIO_FRONTEND_ENGINEERING.md`로 분리

## 2. 마지막 실기 결과

### 2026-07-27 Curve/Reference 개발본 플래시

사용자가 별도로 알리지 않는 한 외부 9V는 분리된다는 합의에 따라 USB 단독 상태에서
COM4에 일반 플래시했다. 전체 삭제, 파티션 변경과 NVS 초기화는 하지 않았다.

- 기본 펌웨어 `0xd2040` bytes, 앱 파티션 18% 여유
- bootloader, factory app, partition table 기록과 SHA 검증 통과
- ESP32-S3 rev0.2, 16MB flash, 8MB PSRAM 80MHz와 PSRAM 메모리 테스트 통과
- `App version: 052f2876-dirty`, 240MHz, ST7796 1.4.0, `boot complete`,
  LVGL task 시작 확인
- 25초 로그에서 ladder IDLE 24회, 3208~3210mV, ratio 0.9996~1.0002
- WDT, panic, 비의도 reset, I2S·메모리 오류 0회
- Curve/Reference 화면과 방향키 조작은 이후 사용자 육안 확인에서 통과

### 2026-07-27 Basic 래더 전압 이동

COM4 장치가 3시간 이상 재부팅 없이 동작하는 상태에서 35초 로그를 수집했다. 사용자는
UP, DOWN, LEFT, RIGHT, OK, HOME을 이 순서로 각각 약 1초간 눌렀다.

| 키 | mV | ratio | 현재 판정 |
|---|---:|---:|---|
| UP | 90 | 0.0280 | DEADZONE |
| DOWN | 252 | 0.0786 | DEADZONE |
| LEFT | 385 | 0.1200 | DEADZONE |
| RIGHT | 633 | 0.1972 | RIGHT |
| OK | 1100~1112 | 0.3427~0.3465 | OK |
| HOME | 1721~1732 | 0.5362~0.5396 | HOME |

무부하는 3209~3210mV로 안정적이었다. 이전 정상 실측보다 모든 눌림 전압이 위로
이동했고 낮은 저항 키에서 상대 영향이 큰 모양이다. 오래 켜 둔 시간만으로 단정하지 않고,
TRS Sleeve/GND 접점 또는 공통 경로에 생긴 작은 직렬저항을 우선 의심한다. 판정창을 넓히면
동시입력을 다른 키로 오인할 수 있어 펌웨어 문턱은 바꾸지 않았다. 양쪽 TRS 플러그 재장착 뒤
같은 1초 로그를 다시 수집하는 것이 다음 분리 시험이다.

### 2026-07-27 Basic 6키 기능 복구

사용자가 6키를 다시 시험했고 모든 키가 정상 동작한다고 확인했다. 이번 확인은 UI 동작
기준이며 ADC mV 재수집은 하지 않았다. 펌웨어 판정창을 변경하지 않은 채 복구됐으므로
앞선 이탈은 일시적인 TRS Sleeve/GND 또는 브레드보드 접점 변화로 우선 분류한다. 현재
차단 요인에서는 해제하되 같은 증상이 재발하면 플러그 재장착 전후 ADC 로그를 비교한다.

### 2026-07-27 Curve/Reference 실기 확인

사용자가 Sound Monitor의 Curve에서 상·하 tilt와 좌·우
DETAIL/BALANCED/SIMPLE 조작이 모두 정상이라고 확인했다. Reference는 `FLAT` 표시,
짧은 HOME과 FOOTSW 응답이 모두 정상이다.

외부 9V가 없는 USB 단독·무입력 상태에서 Reference와 Curve
`0.0dB/oct + DETAIL`은 20~70Hz 부근이 약 -65dBFS로 보였다.
`1.5dB/oct`에서는 70Hz 부근만 약하게 남고 `3.0dB/oct` 이상에서는 표시 하한 아래로
내려갔다. 양의 tilt가 1kHz 아래를 옥타브당 지정 dB만큼 낮추는 계산과 일치한다.
TL072 무전원 상태에서 PCM1808 입력이 부유하므로 이 저역 성분은 유효 입력 성능으로
판정하지 않고 60Hz 주변 전원 험·누설 관찰로 기록한다.

### 2026-07-27 공통 Color/Mode + Nyan 제거 펌웨어

외부 9V가 분리되고 USB만 연결된 상태를 사용자에게 재확인한 뒤 COM4에
`f2182fea`를 일반 플래시했다. 전체 삭제나 파티션 변경은 하지 않았다.

- 빌드·플래시 이미지 `0xd1c40` bytes, 앱 파티션 18% 여유
- bootloader, factory app, partition table 기록 후 각 영역 SHA 검증 통과
- ESP32-S3 rev0.2, 16MB flash, 8MB PSRAM 80MHz와 PSRAM 메모리 테스트 통과
- `App version: f2182fea`, `cpu freq: 240000000 Hz`, ST7796 1.4.0,
  `app: boot complete`, LVGL task 시작 확인
- 정상 리셋 후 약 25초 로그에서 ladder `IDLE` 지속
- WDT, panic, 비정상 reset, I2S 오류, 메모리 오류 0회
- 사용자 실기 확인: Color/Mode 화면과 Classic Cat 장애물 구간의 짧은
  HOME/FOOTSW 응답을 포함해 이상 없음

### 2026-07-26 입력 WDT 핫픽스

2026-07-26 외부 9V를 분리하고 USB만 연결한 상태에서 핫픽스 펌웨어를 COM4에 플래시했다.

- ESP32-S3 rev0.2, 8MB PSRAM 80MHz, 메모리 테스트 통과
- `cpu freq: 240000000 Hz`, `app: boot complete` 확인
- 310초 감시 중 ladder 로그 310회, 첫 로그부터 마지막 로그까지 309초
- task watchdog 0회, panic 0회, 비의도 reset 0회
- 정상 리셋 뒤 15초 재확인에서도 ladder 15회, WDT/panic 0회
- 사용자 육안 확인: Tuner와 Sound Monitor UI가 정상 표시됨
- 사용자 육안 확인: Spectrum 주파수·dBFS 눈금과 방향이 정상임
- 실제 FOOTSW 짧게 1회로 Tuner -> Sound Monitor 전환 성공
- 전환 뒤 20초 추가 감시에서도 ladder 20회, WDT/panic/reset/I2S 오류 0회

핫픽스 전에는 약 5초마다 다음 task watchdog 리셋이 반복됐다.

```text
E task_wdt: IDLE0 (CPU 0)
CPU 0: input
adc_oneshot_read
  -> input_ladder_read_average
  -> input_task
```

당시 화면은 첫 프레임을 완성하지 못한 정적 노이즈 상태였고 버튼은 사용할 수 없었다.

### USB-only 오디오 관찰

외부 9V가 없는 상태에서도 Tuner가 음이름을 표시했고, 브레드보드 소자를 건드리면 음이름이
바뀌었다. Sound Monitor로 전환한 뒤 그래프는 거의 움직이지 않았다.

이 상태에서는 USB가 DevKit의 5V 레일을 통해 PCM1808을 켜지만, 외부 9V가 필요한 TL072는
꺼져 있다. 따라서 PCM1808 아날로그 입력이 부유하며 손 접촉에 따른 주기성 잡음을 받을 수
있다. Tuner의 현재 무음 문턱 `MIN_RMS=0.0002`는 약 -74dBFS라 이 잡음이 clarity 조건까지
통과하면 음정으로 표시된다. 유효한 오디오 입력 상태가 아니므로 지금 문턱을 조정하지 않고,
외부 9V 단독 전원과 정상 입력 신호를 연결한 뒤 재측정한다. 무입력 Sound Monitor가 평평한
것은 현재 관찰과 모순되지 않는다.

### TRS 입력 묶음 실측

35초 동안 ladder 로그 35개를 수집했고 WDT, panic, reset, I2S 오류는 모두 0회였다.

| 상태 | 표본 수 | 최소 mV | 최대 mV | ratio 범위 |
|---|---:|---:|---:|---:|
| IDLE | 11 | 3208 | 3210 | 0.9995~1.0001 |
| UP | 1 | 26 | 26 | 0.0081 |
| DOWN | 5 | 136 | 165 | 0.0424~0.0514 |
| LEFT | 5 | 307 | 313 | 0.0956~0.0975 |
| RIGHT | 4 | 548 | 567 | 0.1707~0.1767 |
| OK | 4 | 1045 | 1062 | 0.3256~0.3309 |
| HOME | 6 | 1664 | 1693 | 0.5184~0.5275 |

모든 표본이 현재 판정창 안에서 올바른 키로 분류됐다. DOWN 최대 165mV와 LEFT 최소
307mV의 간격은 142mV다. 추가 15초 수집에서 UP 26mV, ratio 0.0081을 확인했고 오류는
0회였다. UP 최대 26mV와 DOWN 최소 136mV의 간격은 110mV로 40mV 기준을 통과한다.

사용자 화면 관찰에서도 HOME 롱은 런처, FOOTSW 롱은 Tuner로 전환됐다. 방향키와 각
ladder 입력은 수 초간 올바른 래치를 유지했으며, 반복 주기의 실시간 계산은 코드 감사에서
기존 400ms 지연·120ms 반복을 유지하는 것으로 확인했다.

## 3. 인수 감사에서 교정된 WDT 원인

현재 `CONFIG_FREERTOS_HZ=100`이므로 한 tick은 10ms다. 기존
`pdMS_TO_TICKS(INPUT_POLL_MS)`에서 `INPUT_POLL_MS=5`는 0 tick이 된다. 우선순위 5인
input 태스크가 `vTaskDelay(0)`으로 반복되면서 CPU0의 display, LVGL, IDLE0를 굶긴 것이
직접 원인이다. 매 반복의 ADC 32회 블로킹 읽기는 점유를 악화시켰지만, 0 tick 지연을
놓치면 원인을 완전히 설명할 수 없다.

원 핫픽스 지시서의 "10ms마다 한 샘플을 넣는 32탭 이동평균"은 적용하지 않는다.
그 방식은 320ms 창을 만들고 UP 입력 시 다음 순서로 판정 창을 통과한다.

| 경과 | 이동평균 ratio | 판정 |
|---:|---:|---|
| 150~170ms | 0.5336 -> 0.4715 | HOME 3회 연속 |
| 210~220ms | 0.3471 -> 0.3160 | OK |
| 260~270ms | 0.1916 -> 0.1606 | RIGHT |
| 290ms | 0.0984 | LEFT |
| 320ms | 0.0051 | UP |

HOME이 먼저 래치된 뒤 더 낮은 비율을 무시하는 기존 정책 때문에 UP이 HOME으로 고착될 수
있다. 따라서 짧은 시간에 32샘플을 읽는 기존 버스트 평균은 유지한다.

## 4. 현재 소프트웨어 핫픽스

- `INPUT_POLL_MS`: 5ms -> 10ms
- input 태스크 우선순위: 5 -> 3
- 계산 결과가 0이어도 최소 1 tick을 지연하도록 방어
- ADC 32샘플 버스트 평균, 판정표, 래치, 디바운스, hold/repeat 상수는 유지

10ms 폴에서 hold 500ms, repeat delay 400ms, repeat rate 120ms는 각각 정확히
50/40/12회 누적으로 기존 실시간 값을 유지한다.

## 5. 검증 상태

| 항목 | 상태 |
|---|---|
| 깨끗한 ESP-IDF 전체 빌드 / `-Werror` | 통과 (`pedal_display.bin` 0xefe30 bytes, 6% 여유) |
| `INPUT_TRS_LADDER=0` 컴파일 | 통과 (`0xec820` bytes, 8% 여유) |
| `AUDIO_DUAL_RANGE=1` 컴파일 | 통과 (`0xef9d0` bytes, 6% 여유), 현재 장치에는 미플래시 |
| 호스트 검증 8/8 | 통과 (CTest 7개 + FFT normalization) |
| PC 시뮬레이터 | 깨끗한 빌드·Gallery 포함 전체 결정론적 smoke·Windows 기본 출력 WASAPI 루프백 개방 통과 |
| COM4 탐지 | 2026-07-27 확인 |
| USB 플래시 | Curve/Reference 개발본 통과, 외부 9V 분리·USB 단독 전제 |
| 부팅 주파수 / PSRAM | 240MHz / 8MB 80MHz 확인 |
| WDT 5분 무발생 | 통과 (310초, ladder 310회, WDT/panic/reset 0) |
| 정상 UI 표시 | 통과 (Tuner, Sound Monitor 육안 확인) |
| FOOTSW 짧게 | 통과 (Tuner -> Sound Monitor) |
| 6키 ADC | 7/26 전부 통과, 7/27 일시 이탈 뒤 펌웨어 변경 없이 6키 UI 동작 복구 확인 |
| HOME 롱·FOOTSW 롱 | 통과 (Launcher / Tuner) |
| 3행 런처·Settings·Reorder UI | 통과 (사용자 조작·커서 시인성 확인) |
| 신회로 7상태 ADC 로그 | 통과 |
| USB-only 오디오 | TL072 무전원 부유 입력이라 기능 판정 제외 |
| dB Meter 전압·시간평균 실기 | 사용자 요청으로 보류, 1kHz 1점 교정 대기 |
| 공통 Color/Mode + Nyan 제거 실기 | 플래시·25초 로그·사용자 화면/입력 확인 통과 |
| Curve/Reference 실기 | 이전 tilt 개발본의 단순화·FLAT 표시와 짧은 HOME/FOOTSW 통과. 현재 3단계 FFT·Weighting 개발본은 미확인 |

### 2026-08-05 Sound Monitor 저역 다중 해상도 개발본

- 사용자 20Hz~20kHz sweep 캡처에서 41Hz가 약 20~50Hz 전체에 평면처럼 반복되고,
  108Hz도 실제보다 넓은 저역 덩어리로 보이는 현상을 확인했다. 48kHz/2048-point FFT의
  23.4375Hz bin 하나를 여러 로그 표시점이 공유하던 것이 원인이었다.
- 시각적 저역 감쇠일 뿐 해상도를 늘리지 못하는 `dB/oct` 경로와 Curve 상·하 조작을
  제거했다. 모든 Sound Monitor 모드는 Slope 0인 무가중 dBFS를 받으며 Curve 좌·우의
  `DETAIL/BALANCED/SIMPLE` 공간 평활만 남겼다.
- 원 48kHz FFT는 유지하고, 7-tap triangular anti-alias FIR와 4:1 decimation으로 만든
  12kHz/2048-point 저역 FFT를 20~300Hz에 사용한다. 300~500Hz는 두 분석을 power에서
  혼합하고, FFT bin보다 좁은 로그 표시점은 중심 주파수 power를 보간한다.
- 공유 `fft_map` 회귀 시험에서 41Hz는 peak 40.9Hz / -12dB 폭 19.5Hz,
  108Hz는 105.1Hz / 14.4Hz, 1037Hz는 1041.9Hz / 83.2Hz로 통과했다. 호스트 CTest 6개와
  FFT normalization, PC 결정론 smoke가 통과했다.
- 깨끗한 ESP-IDF `-Werror=all` 기본 빌드는 `0xeed00` bytes(7% 여유),
  `INPUT_TRS_LADDER=0` 빌드는 `0xe3300` bytes(11% 여유)로 통과했다. 오디오 Core1 소유,
  I2S 수집과 seqlock 발행 구조는 바꾸지 않았다.
- 이 개발본은 아직 COM4에 플래시하지 않았다. 본체에서 41/108/1037Hz 또는 sweep 형상,
  Curve 좌·우와 HOME/FOOTSW 응답, Core1 부하·I2S overflow를 확인해야 한다.

### 2026-08-05 Reference 3단계 FFT·중심 정렬·Weighting

- 후속 20Hz~20kHz sweep 캡처에서 저역 피크가 로그 축에서 상대적으로 넓고 오른쪽 경사가
  급하며, 약 500Hz에서 두 해상도의 피크가 분리되는 현상을 확인했다. 고정 길이 FFT의
  절대 bin 폭 때문에 같은 비율 간격이라도 저역 누설이 더 큰 부분은 자연스럽지만,
  170.7ms 저역 창과 42.7ms 고역 창의 서로 다른 중심 시각을 이동 sweep에서 섞은 것은
  실제 구현 결함이었다.
- Reference에 3kHz/2048-point(1.46484375Hz bin) 초저역 분석을 추가했다.
  90~140Hz는 3k→12kHz, 380~520Hz는 12k→48kHz power를 로그 주파수 smoothstep으로
  혼합하며, 48kHz 과거 16개와 12kHz 과거 7개 스펙트럼으로 세 FFT 창 중심을 같은 시각에
  맞춘다. 일반 Monitor도 12k/48kHz 두 창 중심을 맞춘다. 초저역 FFT는 Reference에서만
  실행한다.
- Reference의 65ms 평균과 220ms release를 제거해 sweep 뒤쪽의 과거 저역이 남지 않게
  했다. 2Hz 1차 DC blocker는 0Hz 바이어스를 제거하면서 20Hz 감쇠를 0.05dB 미만으로
  유지한다. 무신호/DC는 `0dBFS`가 아니라 `-72dBFS` 표시 하한으로 내려간다.
- Sound Monitor Settings에 `Weighting=Flat/A-weighted`를 추가했다. Flat이 기본이며,
  A-weighted는 렌더링 복사본에만 적용돼 공통 스냅샷과 dB Meter는 변하지 않는다.
  마이크 SPL 교정이 없으므로 화면에 `dBA`나 ISO 등청감 측정이라고 표기하지 않는다.
- 결정론적 고정 진폭 0.5 시험에서 30/300/3000Hz peak는
  `29.6/301.1/2984.4Hz`, `-7.39/-6.26/-6.02dBFS`였다. 각 +16.7% 지점은
  `-43.22/-72/-72dBFS`였고 -12dB 폭은 `4.0/16.3/81.6Hz`였다. 250~700Hz 이동 sweep
  76개 유효 프레임에서 -6dB 이상의 분리된 두 피크는 없었으며 DC 입력도 바닥으로 복귀했다.
- Curve/Reference의 로그 축에 Sub Bass/Bass/Low-Mid/Mid/High-Mid/High 여섯 구간을
  옅은 색 배경으로 추가하고 `Bass/Mid/High`만 글자로 표시했다. 480×320 PC preview에서
  경계, 그리드, 곡선과 라벨이 겹치지 않는 것을 확인했다.
- 런처 Theme을 `Mode=Dark/Light`, `Color=Blue/Green/Yellow/Red` 두 단계로 분리했다.
  앱 Color는 `Default/Blue/Green/Yellow/Red`이며 고정색도 전역 명암 모드를 따른다.
  NVS schema v6는 같은 blob 크기에서 Color 3비트+Mode 5비트를 사용하고, v5 White와
  Reference 저장값의 색 이전·모드 보존을 호스트 blob 회귀 검사로 확인했다.
- CTest 9/9, FFT normalization, PC 깨끗한 빌드와 추적 EXE 전체 smoke가 통과했다.
  smoke 전후 사용자 `sim/build/sim_nvs.bin` SHA-256은
  `1D30B7C1B6EDA6F27F74C5ECCEC21F54CAEDBE7D5570810F418A6EC7774452B3`으로 같았다.
  ESP-IDF 5.4.4 `-Werror=all` 깨끗한 기본 빌드는 `0xf03b0`(6% 여유),
  `INPUT_TRS_LADDER=0`은 `0xecd70`(7% 여유)로 통과했고 기본 빌드의 정적 D/IRAM은
  87,897 bytes가 남았다. 이 개발본은 플래시하지 않았다.

### 2026-08-05 팝업 계층·Loudness Weighting 개발본

- 런처 Settings의 `Info`를 `About`으로 바꾸고, 앱 홈 메뉴의 `Exit/Settings`를
  `Settings/Info`로 교체했다. 앱 Settings에서는 Info를 제거하고 첫 항목 Theme 아래에
  앱별 `Mode/Color`를 배치한다. Sound Monitor Settings는 `Theme/Weighting`이다.
- Weighting은 기존 인덱스 `Flat=0`, `A-weighted=1`을 보존하면서
  `Flat(Loudness)=2`, `A-weighted(Loudness)=3`을 추가했다. NVS schema는 v6 그대로다.
- Loudness는 1kHz를 0dB로 정규화한 60-phon 등청감 참조 역감도다. 100Hz는 약
  -18.64dB, 3.15kHz는 약 +3.59dB를 표시 복사본에 더하며, 12.5~20kHz는 12.5kHz 보정값을
  유지한다. 실제 청취 SPL을 모르므로 절대 phon/loudness 측정은 아니다.
- 주파수별 보정은 설정 적용 시 256점 캐시를 만들고 매 프레임에는 덧셈만 한다. 원 FFT
  스냅샷, dB Meter, 다른 앱은 변하지 않는다.
- 호스트 CTest 9/9, 깨끗한 PC 시뮬레이터 빌드와 갱신된 추적 `pedal_sim.exe`의 새 메뉴
  계층·4종 Weighting smoke가 통과했다. smoke 전후 사용자 `sim_nvs.bin` SHA-256은
  `622C40B35CFAA0D2B63C2C71703FB95A63342BD466883990F311AE76988CD37F`로 같았다.
- ESP-IDF 5.4.4 기본 `0xf0560`과 `INPUT_TRS_LADDER=0` `0xecf20`이
  `-Werror=all` 전체 빌드를 통과했다. 1MiB 앱 파티션 여유는 기본 6%, 래더 비활성
  7%다. 손상된 공식 Python 환경 대신 작업공간 임시 Python 3.12 환경을 사용했다.
- 이 개발본은 플래시하지 않았다. 본체 팝업 글자 배치, 네 Weighting의 실제 오디오 표시와
  Core1 실시간 부하는 실기 확인 전이다.

### 2026-08-05 Weighting 직접 조작·무음 바닥 수정

- 사용자 관찰에서 Flat 외 세 Weighting을 고르면 무음 상태에서도 2~5kHz가 솟아 보였다.
  FFT release가 표시 하한에 한 픽셀 미만으로 남긴 양수가 해당 대역의 양의 보정으로
  확대된 것이 원인이었다.
- Sound Monitor의 모든 Mode에서 `DOWN=다음`, `UP=이전` Weighting을 바로 적용한다.
  양 끝에서는 멈춰 방향키 오토리피트가 NVS를 계속 순환 저장하지 않는다.
- 원 스냅샷과 Core1 분석은 바꾸지 않았다. 렌더링 복사본에서만 양의 보정을 표시 하한
  6dB 구간에 smoothstep으로 점진 적용하고, 정규화값 0.001 이하는 바닥으로 유지한다.
  `-66dBFS`보다 큰 신호에는 기존 보정 전량을 적용한다.
- 호스트 CTest 9/9와 깨끗한 PC 시뮬레이터 빌드·직접 상하 조작 smoke가 통과했다.
  추적 `pedal_sim.exe`를 갱신했고 smoke 전후 사용자 `sim_nvs.bin` SHA-256은
  `CDA42B43DCDC2C81DC65B48B5B85D1F26F9072564FB10CD784CB393989DA10AC`로 같았다.
- ESP-IDF 5.4.4 기본 `0xf0620`과 `INPUT_TRS_LADDER=0` `0xed000`이
  `-Werror=all` 전체 빌드를 통과했다. 1MiB 앱 파티션 여유는 각각 6%, 7%다.
- 이번 수정은 플래시하지 않았다. 실제 무음 화면, 상·하 6키 입력과 실제 오디오의
  저레벨 Weighting 전환은 본체 실기 확인 전이다.

### 2026-08-05 dB Meter 조작축·Settings 정리

- dB Meter 화면 조작을 `LEFT/RIGHT=INPUT/WINDOW 선택`, `UP/DOWN=선택값 변경`으로
  교체했다. OK로 값을 바꾸던 동작은 제거했다.
- 현재 구형 단일 입력 빌드의 Settings는 `Theme/Input/Window`다. Input은 실제 환산
  이득을 사용한 `LINE 2.00x/INST 7.82x`, Window는 `LIVE/AVG 1s/AVG 3s`를 표시한다.
  화면 직접 조작과 Settings는 같은 setter와 지연 NVS 저장 경로를 사용한다.
- 자동 듀얼레인지 빌드는 하드웨어가 입력 범위를 선택하므로 수동 Input을 노출하지 않고
  `Theme/Window`만 표시한다. Diagnostics 화면에서 Settings로 Window를 바꿔도 생성되지
  않은 일반 컨트롤 객체를 갱신하지 않도록 방어했다.
- 호스트 CTest 9/9와 깨끗한 PC 시뮬레이터 빌드·dB Meter 직접 조작/Settings 상태 일치
  smoke가 통과했다. 추적 `pedal_sim.exe`를 갱신했고 smoke 전후 사용자
  `sim/build/sim_nvs.bin` SHA-256은
  `0766BA4E680CBEF698728E0834333D8CC802A8820A1D62C08A5C3DE9AF0BE26D`로 같았다.
- ESP-IDF 5.4.4 `-Werror=all` 전체 빌드는 기본 `0xf0760`,
  `INPUT_TRS_LADDER=0` `0xed140`, `AUDIO_DUAL_RANGE=1` `0xf1360`으로 모두 통과했다.
  이 개발본은 플래시하지 않았으며 본체 6키와 팝업 화면은 실기 확인 전이다.

## 6. PC 시뮬레이터 자동 확인

### 2026-07-28 Windows 시스템 재생음 입력

- Windows 기본 입력을 SDL 녹음 장치에서 WASAPI 기본 출력 루프백으로 바꿨다. PC에서
  기본 스피커나 헤드폰으로 재생 중인 소리를 모노로 다운믹스하고 48kHz로 변환해 기존
  시각화·튜너·dB Meter·music events 분석 경로에 넣는다.
- 이 PC의 `LF27T450F (NVIDIA High Definition Audio)` 기본 출력이
  `48000Hz stereo -> 48000Hz mono`로 실제 개방됐다.
- `--microphone`은 기존 기본 녹음 입력, `--audio-device N`은 지정 녹음 장치를 사용하며
  `--list-audio`에서 시스템 루프백과 SDL 녹음 장치를 함께 확인할 수 있다.
- VS 2026 임시 깨끗한 빌드와 전체 결정론 smoke가 통과했다. smoke 전후 사용자의
  `sim/build/sim_nvs.bin` SHA-256은
  `8EAEA2CB7F02537D98FD1EF7C14D58A7070E0926DF01954FFA6527599B2B283E`로 같았다.

- `pedal_sim.exe --smoke-test`는 실제 SDL/LVGL 프레임 루프에 내부 UI 이벤트를 주입한다.
- 런처 시작, Sound Monitor 합성 시각화, Images 선택, 짧은 FOOTSW 라이브 순환,
  Tuner 진입·뮤트·공유 DSP 잠금, Bounce 진입, 긴 FOOTSW 퀵 Tuner, 긴 HOME 런처 복귀와
  언뮤트를 검증한다.
- 합성 입력은 두 번 모두 A#3 233.59Hz로 잠겼고 종료 코드 0과 `SMOKE PASS`를 반환했다.
- smoke 모드는 실제 캡처 장치를 열지 않으며 기존 `sim_nvs.bin`의 해시와 수정 시각을
  바꾸지 않았다.
- 공유 DSP의 MIDI, Tuner, FFT 호스트 테스트도 VS 2026에서 3/3 통과했다.

### 2026-07-26 런처 내비게이션 개편

- `LIVE`, `STASH`, `Reorder/Settings` 3행을 빈 행 포함 상하로 순환하고, 좌우는 현재 행
  내부에서만 이동하도록 변경했다.
- Settings 계층(`Theme/Info`)과 Sound Monitor 6개 Theme 프리셋을 추가하고 앱 화면의
  직접 상·하 렌더러/팔레트 변경을 제거했다.
- PC smoke는 빈 STASH 경유, `Settings -> Reorder` 복귀, 전역 테마, 모니터 프리셋,
  기존 라이브 순환/튜너 뮤트/퀵앱까지 통과했다.
- COM4에 기본 TRS 래더 펌웨어를 플래시했다. 12초 부팅 로그에서 `boot complete`, LVGL 시작,
  ladder IDLE 11회, WDT/panic/reset 0회를 확인했다. 외부 9V는 분리된 USB 단독 상태였다.
- 사용자 육안 확인에서 Settings/Reorder 왕복이 정상 동작하고 선택 테두리와 대각 커서가
  잘 보이는 것을 확인했다.

### 2026-07-26 Sound Monitor 스펙트럼 개편

- Curve 화면을 20Hz~20kHz 로그 축과 `-72..0dBFS` 세로축을 갖는 Spectrum 화면으로
  교체했다. 현재 스펙트럼은 평활된 선과 반투명 채움, peak는 별도 잔상선으로 표시한다.
- FFT 표시 기울기를 1kHz 기준 `+4.5dB/oct`로 바로잡고 65ms 파워 평균, 220ms release,
  느린 peak decay를 적용했다. Core1 발행 구조와 오디오 캡처 경로는 변경하지 않았다.
- PC 시뮬레이터의 `--synthetic` 입력을 기타 기본음·배음·피킹 대역 형태로 바꿨고,
  시각 검수와 전체 UI smoke를 통과했다(약 27fps).
- 기본 TRS 래더와 `INPUT_TRS_LADDER=0` ESP 빌드가 모두 통과했다. 첫 플래시에서 렌더러
  정적 작업 배열 때문에 내부 DMA RAM이 부족한 것을 부팅 로그로 발견했고, 배열을 PSRAM으로
  이동한 뒤 재플래시했다.
- 최종 12초 COM4 로그에서 PSRAM 8MB 테스트, ST7796/LVGL, TRS 입력, Spectrum 렌더러
  24~25fps 기동을 확인했다. 메모리 오류, WDT, panic, reset은 없었다.
- 외부 9V가 분리되어 유효한 기타 입력이 없으므로 실제 신호의 주파수 응답과 peak 동작은
  아직 실기 판정하지 않았다.

### 2026-07-26 12밴드/Circular/dB Meter

- 기존 `bars` ID를 유지하면서
  `50/100/200/400/600/800/1.2k/1.6k/3.2k/4.5k/6.4k/10kHz` 12밴드로
  교체했다. `120/500Hz` 대신 사용자 요청의 `600Hz/1.2kHz`를 적용했다.
- 얼굴형 `reactive` 렌더러와 `Talk` 프리셋을 제거하고, PSRAM RGB565 캔버스에서
  방사형 막대를 좌우 대칭으로 그리는 `Circular (Blue/Green)`으로 교체했다.
- `dbmeter` 앱을 추가했다. Core1은 기존 seqlock 스냅샷에 block RMS와 sample peak만
  함께 발행하며, 앱은 RMS·sample peak dBFS와 PCM1808 ADC 핀 기준 명목 Vrms·dBV·dBu를
  표시한다. 프론트엔드 이득 미교정 상태이므로 외부 잭 전압으로 표시하지 않는다.
- 시뮬레이터 `--preview bars|circular|dbmeter`로 세 화면을 각각 픽셀 검수했다.
  12개 눈금과 텍스트 겹침, Circular 비어 있음, dB 단위 간 불일치는 없었다.
- 호스트 audio-level 경계 테스트에서 full-scale sine RMS `-3.01dBFS`, sample peak
  `0dBFS`, `1Vrms=0dBV`, `0.775Vrms=0dBu`를 검증했다.
- 기본 TRS 래더와 실제 `INPUT_TRS_LADDER=0` 구성 모두 깨끗한 ESP-IDF 5.4.4 전체 빌드와
  `-Werror`를 통과했다. 기본 바이너리는 `0xd0d90` bytes, 앱 파티션 여유는 18%다.
- 외부 9V가 분리된 USB 단독 상태에서 기본 펌웨어를 COM4에 플래시했다. 최종 10초 재부팅
  로그에서 8MB PSRAM 테스트, 240MHz, ST7796/LVGL, TRS IDLE, Spectrum 24~25fps를
  확인했고 WDT, panic, reset, 메모리 오류는 없었다.
- 기존 MIDI scene의 `reactive` 대상도 `circular`로 바꿔 프로그램 체인지 경로가
  삭제된 렌더러를 가리키지 않게 했다.
- 첫 실기에서 Circular의 292px·96분할 전체 캔버스 연속 갱신이 Core0를 포화시켜 짧은
  입력을 놓치는 문제가 확인됐다. 240px·72분할·최대 약 16fps로 줄이고, 중심 원 크기를
  고정했으며 스펙트럼 변화가 없으면 재그리기를 멈추도록 수정했다.
- 팝업이 열려 있는 동안 뒤 앱 렌더링을 정지해 모달 입력을 우선한다.
- dB Meter는 50ms마다 RMS 전력을 샘플링해 400ms 시정수로 평균하고, 화면은 200ms마다
  갱신한다. sample peak 숫자는 1초 hold 값을 사용한다.
- 최종 펌웨어 `0xd0680` bytes(앱 파티션 19% 여유)를 COM4에 플래시했다. 사용자 실기
  확인에서 Circular와 dB Meter 모두 짧은 HOME·FOOTSW 입력이 정상 동작했다. 모니터
  로그에서도 WDT, panic, reset은 없었다.

### 2026-07-26 Bounce 고양이 러너

- 기존 피치 위치·바운스 시각화를 Chrome 오프라인 러너 방식의 게임으로 교체했다.
  고정 위치의 픽셀 고양이가 오디오 온셋에 점프하고, 오른쪽에서 다가오는 종이컵을
  피하며 이동 거리 점수와 최고 점수를 기록한다.
- 앱 오디오 요구를 `TUNER`에서 `SPECTRUM`으로 바꿨다. 점프는 공유
  `music_snapshot_t.onset_seq`만 소비하며 Core1 발행 구조는 변경하지 않았다.
- 25fps 시간 기반 물리와 재사용 LVGL 사각형 오브젝트만 사용해 전체 캔버스 재그리기를
  피했다. `OK`·`UP`도 수동 점프와 게임 오버 재시작을 지원한다.
- 시뮬레이터에 `--preview bounce`를 추가했고 480x320 프레임 버퍼를 픽셀 검수했다.
  고양이·종이컵·점수·지면은 겹치지 않았다. smoke는 온셋 점프, 종이컵 충돌,
  게임 오버, 다음 온셋 재시작까지 자동 검증한다.
- ESP-IDF 5.4.4 전체 빌드는 `-Werror`를 통과했고 펌웨어는 `0xd05b0` bytes,
  앱 파티션 여유는 19%다. COM4 플래시 뒤 12초 로그에서 8MB PSRAM, 240MHz,
  ST7796/LVGL, TRS IDLE, 24~25fps를 확인했으며 WDT·panic·reset·메모리 오류는 없었다.
- `sdkconfig.defaults` 기반 깨끗한 기본 빌드와 `INPUT_TRS_LADDER=0` 빌드도 통과했다.
  GPIO 변형은 `0xccf10` bytes, 파티션 여유 20%다. 깨끗한 기본 빌드 중 GCC 14가
  미사용 ESP-DSP convolution 파일에서 병렬 ICE를 냈지만 `ninja -j1` 재개는 통과해
  프로젝트 소스 오류가 아닌 툴체인 병렬 컴파일 문제로 분리했다.
- 외부 9V가 분리되어 실제 기타 입력으로 온셋 임계와 점프 감도를 판정하지 않았다.
  공유 온셋 경로는 시뮬레이터 합성 온셋으로 검증했다.

### 2026-07-26 전역 UI/앱 로컬 Theme 분리

- 런처의 `BLUE/WHITE/GREEN` 전역 UI Theme은 런처와 모든 공통 팝업의 팔레트만
  관리하고, 앱의 `Settings -> Theme`은 해당 앱의 로컬 Theme 훅만 호출하도록 분리했다.
  로컬 Theme이 없는 앱은 Settings에서 `Info`만 표시한다.
- `gadget_app_t`에 선택형 로컬 Theme count/name/index/set 훅을 추가했다. Sound
  Monitor의 기존 6개 프리셋과 Bounce의 `Classic Cat/Nyan Cat`이 이 계약을 사용한다.
- 슬롯 설정에 앱별 `local_theme`을 추가하고 NVS schema를 v3로 올렸다. 기존 v2
  구조체의 패딩 1바이트를 사용해 blob 크기를 유지하며, 컴파일 타임 크기 검사로 호환을
  보장한다. v2의 체인·순서·변형·전역 Theme은 보존하고 새 필드만 0으로 초기화한다.
- MIDI scene이 Sound Monitor 상태를 바꾸면 런타임 슬롯 선택값도 맞춰 Theme 메뉴와
  실제 렌더러가 어긋나지 않게 했다. 사용자가 확인으로 적용할 때만 NVS에 저장한다.
- 별도 임시 폴더의 깨끗한 PC 시뮬레이터 빌드와 smoke를 통과했다. smoke는 전역
  팔레트가 앱 Theme 선택으로 바뀌지 않는 것, Monitor/Bounce 표시값과 슬롯값 일치,
  Nyan 테마의 온셋 점프·충돌·재시작·풋스위치 응답을 검증한다.
- Nyan Cat은 외부 이미지 에셋 없이 LVGL 사각형으로 그렸고 480x320 시뮬레이터에서
  얼굴·팝타르트 몸통·6색 무지개와 점수/지면의 겹침이 없음을 픽셀 검수했다.
- 호스트 테스트 4/4, `sdkconfig.defaults` 기본 및 `INPUT_TRS_LADDER=0` 깨끗한 전체
  빌드와 `-Werror`가 통과했다. 최종 바이너리는 각각 `0xd0d90` bytes(18% 여유),
  `0xcd6f0` bytes(20% 여유)다.
- 사용자가 외부 9V 분리·USB 단독 상태를 확인한 뒤 기본 펌웨어를 COM4에 플래시했다.
  12초 로그에서 ESP32-S3 rev0.2, 8MB PSRAM 80MHz 메모리 테스트, 240MHz,
  ST7796/LVGL, TRS ladder IDLE, Sound Monitor 24~25fps를 확인했다. WDT, panic,
  비정상 reset, 메모리 오류는 없었다.
- 전역 Theme과 앱 팝업 팔레트 일치 및 Nyan Cat 선택의 실기 육안 확인은 사용자 요청으로
  보류했다. 자동 UI smoke와 PC 픽셀 검수는 통과한 상태다.

### 2026-07-26 dB Meter 입력 전압과 시간 평균

- PCM1808의 `3Vpp` single-ended full-scale을 기준으로 ADC 핀 RMS 전압을 계산하고,
  조립된 TL072 프론트엔드의 수동 선택 이득 `LINE 2.00x` / `INST 7.82x`로 나눠 입력
  잭의 AC RMS 전압을 명목 추정한다. 앱의 INPUT 선택은 실제 SPDT 위치와 사용자가 맞춘다.
- 별도 전압 측정 회로는 가청 대역 AC 오디오 측정에 필요하지 않다. 다만 입력 커플링과
  PCM1808 HPF를 거친 디지털 표본이므로 DC 전압은 측정하지 못하며, SPDT 위치 자동 감지는
  별도 GPIO 배선 없이는 불가능하다.
- `LIVE`는 최신 256표본 블록 RMS, `AVG 1s`와 `AVG 3s`는 Core1이 누적 발행한 모든
  표본의 제곱합과 개수를 이용한 최근 구간 RMS다. dB 값을 산술 평균하지 않고 전력을
  평균하며, 화면 수치 갱신은 가독성을 위해 200ms를 유지한다.
- LINE/INST와 평균 모드는 dB Meter의 앱 옵션으로 NVS에 저장한다. schema를 v4로 올렸고
  기존 v2/v3의 슬롯·체인·변형·전역/로컬 Theme은 보존한 채 새 옵션만 0으로 초기화한다.
- 전압 보정계수는 현재 LINE/INST 모두 `1.0`이다. 저항 오차, PCM1808 모듈 편차와
  프론트엔드 주파수 응답을 포함한 정확도는 알려진 1kHz Vrms 신호로 1점 교정한 뒤 확정한다.
- PC 시뮬레이터 빌드와 UI smoke, 호스트 테스트 4/4가 통과했다. `sdkconfig.defaults`
  깨끗한 전체 빌드는 `0xd1540` bytes(18% 여유), `INPUT_TRS_LADDER=0` 빌드는
  `0xcdec0` bytes(20% 여유)로 모두 `-Werror`를 통과했다.
- 사용자가 실기 확인을 보류했고 현재 USB 단독이라 TL072가 꺼져 있으므로 플래시와 실제
  전압 판정은 수행하지 않았다. 마지막 실기 펌웨어 기준은 위 Theme 분리 빌드 그대로다.

### 2026-07-27 공통 Color/Mode와 Nyan 제거

- 사용자가 마지막 실기 펌웨어에서 전역 Theme 적용과 앱 설정 팝업 팔레트 연동이
  정상임을 확인했다. 같은 빌드의 Nyan Cat은 장애물이 등장하면 화면이 버벅이고 짧은
  키 입력을 거의 받지 못하는 실기 회귀가 확인됐다.
- Nyan Cat Mode와 전용 얼굴·몸통·무지개 LVGL 오브젝트 및 매프레임 갱신 경로를
  제거했다. Bounce Mode는 `Classic Cat` 하나만 남겼다.
- 모든 앱 Settings를 `Color/Mode/Info`로 통일했다. Color는
  `Default/Blue/White/Green`이며 Default는 런처 Theme을 상속한다. 고정 Color는 앱
  콘텐츠만 바꾸고 런처와 공통 팝업은 계속 전역 Theme을 따른다.
- Sound Monitor는 색과 독립된 `Spectrum/12-Band/Circular` Mode를 제공한다. Images,
  Tuner, Bounce, dB Meter도 향후 확장 위치가 흔들리지 않도록 각각 한 개의 명명된
  Mode를 제공한다.
- NVS schema는 v5다. 기존 8바이트 앱 설정 blob의 외형 1바이트를 Color 2비트와 Mode
  6비트로 재사용한다. v2~v4 설정 크기를 유지하며 Monitor 6프리셋은 대응 Color/Mode로,
  이전 Nyan 선택은 `Default + Classic Cat`으로 변환한다.
- 깨끗한 PC 시뮬레이터 빌드·결정론 smoke와 Color/Mode 세 페이지의 480x320 시각 검수,
  호스트 테스트 4/4가 통과했다. `sdkconfig.defaults` 전체 빌드는 `0xd1c40` bytes
  (18% 여유), `INPUT_TRS_LADDER=0` 빌드는 `0xce5c0` bytes(19% 여유)로 둘 다
  `-Werror`를 통과했다.
- 외부 9V 분리·USB 단독 상태에서 `f2182fea`를 COM4에 플래시했다. 정상 리셋 뒤
  약 25초 동안 rev0.2, 8MB PSRAM 80MHz, 240MHz, ST7796/LVGL, ladder `IDLE`을
  확인했고 WDT·panic·비정상 reset·I2S·메모리 오류는 없었다. 화면 외형과 Classic Cat
  장애물 구간의 HOME/FOOTSW 짧은 입력도 사용자가 실기 확인했고 이상이 없었다.

### 2026-07-27 Curve/Reference 개발본

- 기존 Monitor Mode 인덱스 0~2와 렌더러 id `curve`를 보존하면서 표시명 Spectrum을
  Curve로 바꾸고 Reference를 인덱스 3에 추가했다.
- Curve는 상·하로 `0/1.5/3.0/4.5/6.0dB/oct`, 좌·우로
  `DETAIL/BALANCED/SIMPLE`을 바꾼다. 기존 `options` 바이트의 미사용 비트에 valid
  sentinel과 설정을 저장해 NVS schema v5 크기를 바꾸지 않았다.
- Reference는 0dB/oct와 공간 평활 없음으로 고정한다. Core1이 소유한 `fft_map`에는
  원자적 tilt 요청만 전달하고, 기존 seqlock 발행 구조는 바꾸지 않았다.
- VS 2026의 깨끗한 PC 시뮬레이터 빌드와 결정론 smoke가 통과했다. smoke는 Curve
  기울기·단순화, Reference flat 선택, 기존 런처·앱·튜너·Bounce·dB Meter 흐름을 확인했다.
- ESP-IDF 기본/래더 비활성 전체 빌드, COM4 일반 플래시와 25초 부팅 로그가 통과했다.
  이후 사용자 실기에서 Curve/Reference 화면과 방향키 응답도 통과했다.

### 2026-07-27 자동 듀얼레인지 캡처 소프트웨어

- 기본 `AUDIO_DUAL_RANGE=0`은 현재 구형 브레드보드의 PCM1808 VINL 단일채널을 그대로
  읽는다. 목표 듀얼레인지 하드웨어 전에는 이 기본값을 유지한다.
- `AUDIO_DUAL_RANGE=1` 변형은 32-bit stereo I2S의 left=HOT, right=SENSITIVE를
  동시에 읽고 `audio_autorange`가 두 경로를 고정 GG 입력 스케일로 환산한다.
- SENSITIVE ADC peak 0.82에서 HOT으로 전환하고, 0.45 아래가 500ms 지속돼야
  SENSITIVE로 복귀한다. 클리핑 전환을 제외한 변경 블록에는 crossfade를 적용한다.
- dB Meter는 이 변형에서 수동 LINE/INST 대신 `AUTO SENSITIVE/HOT`과 clip 상태를
  표시하며, dBFS와 입력 Vrms·dBV·dBu를 명목 GG Input Full Scale로 계산한다.
- 호스트 autorange 테스트는 스케일 일치, 즉시 HOT 전환, hysteresis 복귀, clip 우회와
  비유한값 정리를 통과했다. CTest 4개와 별도 FFT normalization 검사까지 5/5 통과했다.
- ESP-IDF `-Werror=all` 기본 `0xd2190`, 래더 비활성 `0xceb10`, 듀얼레인지 `0xd2e60`가
  모두 통과했고 PC 시뮬레이터 전체 smoke도 통과했다.
- 현재 하드웨어는 VINR SENSITIVE가 준비되지 않았으므로 듀얼레인지 이미지는 플래시하지
  않았다. 실제 두 채널 순서·overlap 일치·전환 연속성·I2S overflow는 목표 회로 실장 뒤 검증한다.

### 2026-07-27 자동 듀얼레인지 조립·교정 준비 (당시 Step 5B)

- 듀얼 빌드의 dB Meter Mode에 `Range Diagnostics`를 추가했다. HOT=VINL와
  SENSITIVE=VINR의 block RMS/peak dBFS, 각 범위의 입력잭 환산 Vrms, raw S/H와 교정 후
  mismatch를 한 화면에서 표시한다. 외부 9V 단독 운용 중 USB 로그 없이 측정하기 위한
  작업 화면이며 기본 `AUDIO_DUAL_RANGE=0` 빌드에는 노출되지 않는다.
- 1kHz scalar correction과 GG Input Full Scale을 CMake cache 변수로 주입할 수 있게 했다.
  실제 값은 알려진 Vrms 측정 전까지 1.0/명목값을 유지한다.
- OPA2192의 전형적 공통모드 입력 용량 6.4pF를 반영해 HOT 보상 시작값을
  `10M||3.3pF : 1.5M||15pF`로 정정했다. 고임피던스/pF 노드는 솔더리스 브레드보드가
  아니라 세척한 만능기판 또는 PCB에 실장하고, 최종 C값은 sweep으로 정한다.
- 당시 `ASSEMBLY.md`에 구형 기준 측정과 목표 회로 개조·교정 절차를 구체화했다.
  2026-08-03 문서 정리에서 이 내용은 `hardware/AUDIO_FRONTEND_ENGINEERING.md`로 옮겼다.
- 현재 변경분으로 ESP-IDF 기본·`INPUT_TRS_LADDER=0`·`AUDIO_DUAL_RANGE=1`
  `-Werror=all` 전체 빌드, CTest 4개, FFT normalization, PC 시뮬레이터 smoke를 다시
  통과했다. 듀얼 이미지는 `0xd2e60`이고 1MiB 앱 파티션에 18%가 남는다.
- 이 항목은 소프트웨어·문서 준비 기록이다. 외부 9V 측정, TL072 해체, OPA2192 실장,
  듀얼 이미지 플래시는 아직 수행하지 않았다.

### 2026-07-28 본체/PC 시뮬레이터 스펙트럼 SSOT 통합

- 시뮬레이터에만 있던 256-point DFT는 48kHz에서 bin 간격이 187.5Hz였고, 별도 peak
  감쇠를 사용해 본체의 2048-point 분석보다 저역 해상도와 잔상 동작이 달랐다.
- 별도 매핑을 제거하고 본체와 시뮬레이터가 같은 `fft_map.c`를 직접 빌드하도록 바꿨다.
  PC에는 ESP-DSP 호출 계약을 구현하는 portable FFT 실행 백엔드만 추가했다. 따라서
  23.4375Hz bin, 로그 매핑, 65ms 평균, 즉시 attack, 220ms release와 peak hold가 같다.
- 46.875Hz 입력의 저역 위치와 peak 지속을 검사하는 공통 DSP 호스트 테스트를 추가했다.
  CTest 5/5와 별도 FFT normalization, 깨끗한 PC 시뮬레이터 빌드와 smoke,
  Windows 기본 출력 `LF27T450F`의 48kHz 루프백 개방, ESP-IDF 기본 `0xd2190`·래더
  비활성 `0xceb10` 전체 `-Werror` 빌드가 통과했다. smoke 전후 사용자
  `sim_nvs.bin` SHA-256은 `A2F43E...BE6`으로 같았다. 이번 작업에서는 실기 플래시를
  하지 않았다.
- 앞으로 앱·UI·공유 DSP 변경은 깨끗한 시뮬레이터 빌드·smoke와
  `sim/build/pedal_sim.exe` 갱신을 같은 작업의 완료 조건으로 삼는다.

### 2026-07-28 SD 콘텐츠 기반과 Gallery

- LCD SPI2의 G12 SCLK·G13 MOSI를 공유하고 G11 MISO·G47 CS를 쓰는 10MHz
  SDSPI/FATFS 백엔드를 구현했다. Gallery가 처음 필요할 때만 마운트하므로 카드가 없어도
  부팅과 다른 앱은 유지된다.
- 공통 저장소 API가 `GG/images`, `GG/music`, `GG/roms`를 파일명 순으로 최대 64개
  열거한다. 경로 순회를 거부하고 이미지·음악·Retro-Go급 ROM 확장자를 구분한다.
- 기존 `images` 안정 ID와 NVS 슬롯은 유지하면서 표시명을 Gallery로 바꿨다.
  JPG/JPEG/PNG/BMP/GIF/LVGL BIN을 실제 파일에서 열고 좌·우 탐색, OK 재검색을 지원한다.
  Music은 카탈로그만 준비됐고 코덱 전에는 재생하지 않는다. Retro도 ROM 카탈로그만
  준비했으며 코어·GPLv2·파티션 결정 전에는 실행하지 않는다.
- PC 시뮬레이터는 기본 `sim/sdcard` 또는 `GG_SD_ROOT` 폴더를 같은 저장소 API로 읽는다.
  깨끗한 빌드와 Gallery를 포함한 전체 smoke가 통과했고, 추적 EXE를 갱신했다.
  smoke 전후 사용자 `sim/build/sim_nvs.bin` SHA-256은
  `A2F43E17667F70DA1D002B842B16417A239ACA768F3CC09E6369460450A77BE6`으로 같았다.
- CTest 6/6과 FFT normalization, 깨끗한 ESP-IDF 기본·래더 비활성·듀얼레인지
  `-Werror` 빌드가 통과했다. 이미지 디코더 추가로 1MiB 앱 파티션 여유가 6~8%까지
  줄었으므로 파티션 변경은 별도 승인 전까지 하지 않는다.
- 이번 작업에서는 실기 플래시와 SD 어댑터 장착·마운트·Gallery 화면 확인을 하지 않았다.
  실제 PNG를 지정한 PC preview 프로세스는 정상 유지됐지만 자동 화면 캡처 승인이
  시간 초과되어 픽셀 육안 판정은 기록하지 않는다.

### 2026-07-28 SD 모듈 식별 정정

- 사용자가 실제 부품을 `SZH-EKBZ-005`로 정정했다. 이 모듈은 순수 3.3V 소켓이 아니라
  4.5~5.5V VCC, 온보드 3.3V LDO와 레벨 변환 회로를 갖춘 6핀 SPI 모듈이다.
- 하드웨어 계약을 VCC=`+5V`, GND=공통, MISO=G11, MOSI=G13, SCK=G12, CS=G47로
  바로잡았다. `+5V`는 VCC에만 쓰고 GPIO는 3.3V 로직을 유지한다.
- 모듈은 아직 장착하지 않았고 이번 정정에서 빌드·플래시·실기 검증은 수행하지 않았다.
  첫 브링업은 기존 전원 전제대로 외부 9V를 분리한 USB 단독 상태에서 수행한다.

### 2026-07-28 시뮬레이터 Curve peak·무음 release 수정

- Sound Monitor의 공통 FFT peak envelope는 유지하되 렌더러 프레임에는 12-Band에서만
  전달한다. Curve와 Reference는 현재 스펙트럼 선·채움만 표시하고 주황색 peak 잔상선을
  표시하지 않는다.
- Windows WASAPI 루프백은 재생이 완전히 멈추면 무음 패킷도 보내지 않을 수 있었다.
  패킷이 50ms 이상 비는 동안 경과 시간만큼 48kHz 무음 표본을 시뮬레이터 큐에 보충해
  `fft_map.c`의 65ms 평균과 220ms release가 끝까지 진행되도록 했다.
- 새 FFT 회귀 검사는 신호 뒤 1초 동안 peak hold가 현재선보다 오래 남는 것과, 추가 4초
  무음 뒤 현재선·peak가 모두 표시 바닥으로 복귀하는 것을 함께 확인한다.
- VS 2026 시뮬레이터 깨끗한 재구성·빌드와 전체 smoke, CTest 6/6, 별도 FFT normalization,
  ESP-IDF 5.4.4 기본 `0xeed10`·`INPUT_TRS_LADDER=0` `0xeb730` 전체
  `-Werror=all` 빌드가 통과했다. 추적 `pedal_sim.exe`를 갱신했으며 smoke 전후
  `sim_nvs.bin` SHA-256은 `A2F43E...BE6`으로 같았다.
- 이번 작업에서는 본체 플래시를 하지 않았다. 실제 Windows 시스템 오디오의 재생/정지
  육안 확인은 갱신된 시뮬레이터에서 수행할 항목이다.

### 2026-08-03 현재 배선 문서 정리와 물리 상태 갱신

- 사용자 보고로 `SZH-EKBZ-005` SD 어댑터 배선 완료를 기록했다. 카드 마운트, Gallery,
  LCD 공유 SPI 안정성은 아직 실기 확인하지 않았다.
- 오디오 입력 구간은 현재 재배선이 필요한 상태다. 사용자가 보유하지 않은 OPA2192 목표
  회로를 현재 조립 지시에서 제외하고, 보유한 TL072+SPDT 회로를 현재 배선 기준으로 유지했다.
- `ASSEMBLY.md`를 준비물·단계별 조립·해체 지시·폐기 회로가 없는 작업대용 연결표로
  재작성했다. 미래 자동 듀얼레인지의 설계·조달 조건·측정·교정 메모는
  `hardware/AUDIO_FRONTEND_ENGINEERING.md`로 분리했다.
- 이번 갱신은 사용자 물리 보고와 문서 정리다. SD 기능 시험, 오디오 재배선, 외부 9V
  레일 측정, 펌웨어 빌드·플래시를 수행하지 않았다.

### 2026-08-05 As-Built 분리와 PC 우선 개발 결정

- 사용자가 실물 회로는 Markdown으로 직접 관리하고, Fritzing은 실물 위치·점퍼 색을 보는
  보조 도면, KiCad는 향후 PCB 생산용 목표 회로로 사용하기로 결정했다.
- `hardware/AS_BUILT_WIRING.md`를 실물 배선 SSOT로 만들었다. 확인된 화면·I2S·TRS·
  풋스위치·SD 연결만 기록하고 현재 정확한 끝점을 모르는 오디오 구간은 미확인으로 남겼다.
- `ASSEMBLY.md`는 실물 기록이 아니라 Codex가 제안하는 다음 적용 배선으로 역할을 바꿨다.
  권장안을 실물에 반영한 사실이 확인되기 전에는 As-Built로 자동 승격하지 않는다.
- PC 시뮬레이터를 앱·UI·설정의 기준 구현이자 GG 없이도 쓸 수 있는 독립 앱으로 개발한다.
  PC의 오디오 loopback·로컬 폴더·향후 오디오 출력이 PCM1808·MicroSD·코덱을 대체하고,
  앱과 상태 기계는 본체와 같은 공통 소스를 사용한다.
- 현재 PC에서도 Music과 Retro는 카탈로그만 있고 실제 재생·실행은 없다. 다음 소프트웨어
  순서는 기존 UI/설정 감사, 플랫폼 능력, PC 오디오 출력, Music, Metronome, Gallery UX,
  공통 게임 순이다. 상세 계약은 `PC_SIMULATOR_PRODUCT.md`에 기록했다.
- 이번 작업은 문서와 개발 계약 정리이며 코드·추적 EXE·본체 펌웨어를 변경하지 않았다.

### 2026-08-05 Sound Monitor 1차 최적화와 SD 앱 폴백 계약

- Sound Monitor를 최우선으로 두고 Curve/Reference에서 이미 표시하지 않던 peak 계산과
  상태 추적을 제거했다. Curve 목표 주기는 33.3ms에서 30ms로 줄였고, 결정론적 PC smoke의
  측정값은 약 28fps에서 31~32fps로 개선됐다. 12-Band의 peak 표시는 그대로 유지한다.
- Gallery는 이미지나 SD 루트가 없을 때 어두운 `GG` 월페이퍼와 상태 문구를 표시한다.
  smoke가 이 폴백 화면을 실제 객체 상태로 검사하도록 확장했다.
- 저장소의 Retro 명칭을 Game으로 바꾸고 표준 경로를 `GG/games`로 정했다. Game의
  빈 타일→내장 점프 게임과 Music의 내장 8비트 로비 음악은 제품 계약을 확정했으며,
  실제 앱·재생 구현은 다음 단계다. 앱은 한 번에 하나만 실행하고 Bluetooth audio는
  GG 범위에서 제외한다.
- 깨끗한 PC Debug 빌드와 갱신된 추적 `pedal_sim.exe` smoke, 호스트 CTest 6/6,
  FFT normalization이 통과했다. ESP-IDF 5.4.4 기본 `0xeec00`과
  `INPUT_TRS_LADDER=0` `0xeb620`도 `-Werror=all` 전체 빌드를 통과했다.
- 로컬 ESP-IDF 공식 Python 환경은 삭제된 Python 3.14 경로를 가리켜 동작하지 않았다.
  전역 설치를 건드리지 않고 작업공간의 임시 Python 3.12 환경으로 검증했으며, 정리 후
  다음 일상 빌드·플래시 전에 공식 환경을 복구해야 한다.
- 이번 작업에서는 본체 플래시와 실기 검증을 수행하지 않았다.

### 2026-08-06 12-Band/Circular 최적화 개발본

- renderer 공통 디스패치에 호출·redraw 횟수와 평균·최대 실행 시간을 누적하는 계측을
  추가하고, 시뮬레이터에 합성 sweep과 HOME 팝업 응답을 함께 재는
  `--renderer-benchmark`를 추가했다.
- 12-Band는 프레임마다 반복하던 밴드 경계 로그 계산과 색 혼합을 캐시하고, 막대 opacity를
  32단계로 양자화해 LVGL 스타일 갱신 비용을 줄였다.
- Circular는 72방향 값을 37개 좌우 공유점으로 줄이고 sample 보간 위치를 캐시했다.
  굵은 선을 픽셀 원 stamp로 그리던 경로는 평행 Bresenham 선으로 교체했으며, 직전·현재
  반경의 합집합만 지우고 무효화한다. 오른쪽 픽셀을 왼쪽에 복제해 색과 형상을 완전히
  대칭으로 만들었고 위쪽은 20kHz, 아래쪽은 20Hz다.
- 최적화 전 PC 기준 12-Band/Circular renderer 평균은 약 166/195us였고, 깨끗한 최종
  빌드 계측은 약 164/111us였다. 12-Band redraw는 23.3Hz로 유지됐고 Circular는
  15.5Hz였다. HOME 열기·닫기는 3~7ms, Circular 좌우 픽셀 불일치는 0이었다.
  자동 수용선은 10/8Hz, 최대 50ms, HOME 120ms다.
- 깨끗한 PC Debug 빌드의 benchmark와 smoke, 호스트 CTest 9/9가 통과했다. smoke 전후
  추적 `sim_nvs.bin` SHA-256은 `0766BA...E26D`로 같았다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 `-Werror=all` 빌드가 통과했다.
  이미지는 각각 `0xf0ac0`(1MiB 앱 파티션 6% 여유), `0xed4c0`(7% 여유)다. 임시 Python
  3.12 환경을 사용했고 `ESP_ROM_ELF_DIR` 부재로 gdbinit 생성 경고만 남았다.
- 추적 `pedal_sim.exe`를 같은 소스로 갱신했다. 본체 플래시와 실기 화면 확인은 수행하지
  않았다.

### 2026-08-06 D1 플랫폼 능력·PC 오디오 출력 기반

- `gadget_app_t.needs_codec`를 `required_capabilities`로 교체했다. 플랫폼은 화면·분석 입력·
  재생 출력·저장소·MIDI·게임 런타임 능력 비트의 타입을 제공하고, 매니저는 런처 표시뿐
  아니라 직접 진입·부팅 복원·풋스위치 라이브 순환에서도 가용 앱만 선택한다.
- `audio_playback.*`에 48kHz 스테레오, 단일 앱 ID 소유권, 재생/일시정지/정지,
  Music/Effects 큐와 버스·마스터 gain 및 최종 클리핑을 구현했다. 메인 기타 Thru는 이
  capability와 믹서에 포함하지 않았다.
- PC의 `sim_audio_output.*`는 SDL queued output을 사용한다. 출력 장치가 실제 개방된
  경우만 `AUDIO_PLAYBACK_OUTPUT`을 제공하며 `--output-device N`과 출력 장치 열거를
  지원한다. smoke에서는 실제 소리를 내지 않는 가상 sink로 256 프레임의 좌우가 다른
  스테레오 신호와 소유권 해제를 검증한다.
- 현재 Windows 세션의 SDL 장치 열거에서는 활성 재생 endpoint가 0개여서 실제 청음은
  확인하지 못했다. 이 경우 장시간 개방을 기다리지 않고 즉시 unavailable로 내려가며,
  재생 장치가 보이는 환경에서의 실제 SDL 출력·청음은 D2 Music과 함께 확인한다.
- 현재 ESP는 재생 capability 없이 공통 API를 명시적 `UNAVAILABLE`로 초기화하며 큐 메모리를
  할당하지 않는다. 미래 코덱 백엔드는 Core1에서 같은 render API를 소비한다.
- 호스트 CTest 10/10, 깨끗한 PC Debug 빌드와 전체 smoke가 통과했다. smoke 전후 사용자
  `sim/build/sim_nvs.bin` SHA-256은 `0766BA...E26D`로 같았다.
- ESP-IDF 5.4.4 공식 PowerShell 환경이 다시 정상 동작했다. 깨끗한 기본과
  `INPUT_TRS_LADDER=0` `-Werror=all` 빌드가 통과했고 이미지는 각각 `0xf0d20`
  (1MiB 앱 파티션 6% 여유), `0xed710`(7% 여유)다. 래더 비활성 첫 병렬 빌드에서 외부
  `esp-dsp` 한 파일의 GCC 내부 오류가 한 번 발생했으나 같은 개체 재실행은 통과했다.
- 이번 단계는 재생 기반만 구현했다. Music UI·로비 음악·WAV 디코더는 다음 D2 범위이며,
  본체 플래시와 실제 스피커/헤드폰 청음은 수행하지 않았다.

### 2026-08-06 D2 Music 앱·WAV 재생

- `music` 앱을 레지스트리에 추가했다. PC 출력 장치가 열려 `AUDIO_PLAYBACK_OUTPUT` 능력이
  있을 때만 런처와 라이브 체인에 나타나며, 코덱 없는 현재 ESP에서는 비활성이다.
- `GG/music`에서 음악 파일을 정렬 탐색한다. WAV는 PCM 8/16/24/32-bit와 float32,
  mono/stereo, 8~192kHz를 순차 읽어 48kHz stereo로 변환한다. MP3/FLAC/OGG는 목록에는
  나타나지만 선택 시 아직 지원하지 않는다는 오류를 명시한다.
- 음악 파일이 없으면 저작권 외부 자산 없이 코드로 생성한 8초 E-minor 8비트 임시 로비
  트랙을 반복 재생한다. 좌우=이전/다음, OK=재생/일시정지, 상하=5% 볼륨 조정이며 진행률·
  시간·소스·WAV 형식·오류 상태를 표시한다. 앱 종료 시 출력 큐와 소유권을 해제한다.
- `AUDIO_NONE`을 추가해 Music 실행 중 Core1의 I2S 수신 주기는 유지하면서 tuner/FFT/meter
  분석만 건너뛴다. 오디오 발행과 하드와이어 메인 기타 Thru 계약은 바꾸지 않았다.
- Music preview를 육안 확인해 480x320 화면의 제목·볼륨·트랙·진행률·조작 아이콘 겹침과
  폰트 문제를 수정했다. 자동 smoke는 가상 stereo sink에서 로비 출력의 비영점 샘플, 진행,
  일시정지/재개, 볼륨과 앱 전환을 확인했다.
- 호스트 CTest 11/11, 깨끗한 PC Debug 빌드와 전체 smoke가 통과했다. 추적
  `sim/build/pedal_sim.exe`를 같은 소스로 갱신했고 사용자 `sim_nvs.bin` SHA-256은
  `0766BA...E26D`로 유지됐다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` `-Werror=all` 전체 빌드가 통과했다.
  이미지는 각각 `0xf3120`(1MiB 앱 파티션 5% 여유), `0xefb20`(6% 여유)다.
- 현재 Windows 세션에서 HDMI와 모니터 재생 endpoint 2개가 열거됐고 0번 장치를
  48kHz stereo로 실제 개방하는 데 성공했다. Music 샘플의 가상 출력 검증과 실제 장치 개방은
  통과했지만 사람이 들리는 소리를 판정하는 청음 확인과 본체 플래시는 수행하지 않았다.

### 2026-08-06 D3 Game 타일 로비·내장 Cat Run

- `game` 앱을 7번째 앱으로 등록했다. 가운데에 96x96 정사각형 슬롯 4개를 배치하고 좌우로
  선택한다. 실행 가능한 외부 코어가 아직 없으므로 `GG/games` 파일은 탐색만 하며 타일에는
  표시하지 않는다. 빈 타일에서 OK를 누르면 내장 Cat Run을 시작한다.
- Bounce의 점프·충돌·오디오 onset 런타임을 `bounce_game.*` 경계로 공유한다. Game은 로비와
  상태 전환만 소유하고, 내장 게임의 HOME은 선택 타일을 보존한 로비로 돌아간다. 로비의
  HOME은 기존 표준 앱 메뉴를 연다.
- Game preview를 육안 확인해 480x320 화면의 네 슬롯, 정사각형 비율, 선택 테두리·커서,
  제목과 소스 표시가 겹치지 않음을 확인했다. 자동 smoke는 타일 이동, 빈 타일 진입,
  내장 Cat Run 점프, HOME 로비 복귀와 선택 위치 보존을 검사했다.
- 호스트 CTest 11/11, 깨끗한 PC Debug 빌드와 추적 `sim/build/pedal_sim.exe` 전체 smoke가
  통과했다. 추적 사용자 `sim/build/sim_nvs.bin`은 작업 전 SHA-256
  `0766BA...E26D`로 복원·보존했다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 빌드가 통과했다. 이미지는 각각
  `0xf3870`(1MiB 앱 파티션 5% 여유), `0xf02a0`(6% 여유)다.
- 외부 9V, USB 장치, 본체 플래시와 실기 버튼·화면 검증은 이번 단계에서 건드리지 않았다.

### 2026-08-06 D4 Metronome·앱 효과음

- `metronome`을 8번째 앱으로 등록했다. 48kHz sample-clock 엔진이 40~220 BPM,
  2~5박자와 Quarter/Eighth/Triplet/Sixteenth tick을 만들며 첫 박을 더 강하게 표시·재생한다.
  앱은 정지 상태로 시작하고 OK=시작/정지, 좌우=Tempo/Meter/Division 선택,
  상하=값 변경이다. 직접 조작과 Settings는 같은 상태를 쓰며 BPM·박자·분할을 지연 저장한다.
- PC에서 `AUDIO_PLAYBACK_OUTPUT`을 사용할 수 있으면 Metronome click이 공통 Effects 버스로
  출력된다. 현재 코덱 없는 ESP에서는 앱을 숨기지 않고 `VISUAL` 무음 모드로 동작시킨다.
- Bounce와 Game의 수동 점프·장애물 통과·충돌에 코드 생성 효과음을 연결했다. 앱이 실행
  중일 때만 재생 소유권을 잡고 종료 시 해제하며, PC loopback이 자기 효과음을 오디오 점프로
  다시 감지하지 않도록 출력 뒤 120ms onset 억제를 둔다.
- Metronome preview를 480x320에서 확인해 BPM, 4개 beat box, subdivision 점, 재생 아이콘과
  아래 3개 조작부가 겹치지 않고 중앙 정렬됨을 확인했다. 시작·정지와 설정 상태는 결정론적
  smoke에서 추가로 검사했다.
- 호스트 CTest 13/13과 FFT normalization, 깨끗한 PC Debug 빌드와 전체 smoke가 통과했다.
  추적 `sim/build/pedal_sim.exe`를 같은 소스로 갱신했고 사용자 `sim_nvs.bin` SHA-256은
  `0766BA4E680CBEF698728E0834333D8CC802A8820A1D62C08A5C3DE9AF0BE26D`로 유지됐다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 `-Werror=all` 빌드가 통과했다. 이미지는
  각각 `0xf4f10`(1MiB 앱 파티션 4% 여유), `0xf1920`(6% 여유)다. 다음 대규모 앱 추가 전에
  승인된 파티션 확장 계획이 필요하다.
- 본체 플래시, 실제 헤드폰 청음과 실기 버튼·화면 검증은 수행하지 않았다. 외부 9V와 장치
  연결 상태도 변경하지 않았다.

### 2026-08-06 D5 Gallery 미디어 UX

- Gallery에 Scanning/Loading/Ready/Empty/Error 상태를 추가했다. 빈 폴더·저장소 부재는
  어두운 `GG` 월페이퍼를 유지하고, 손상 파일은 앱을 중단하지 않는 항목별 오류 화면으로
  표시한다.
- 공통 저장소 스캔은 허용 파일 전체를 확인한 뒤 대소문자 무시 자연 정렬하고 최대 64개를
  표시한다. 긴 경로와 상한 초과를 별도로 집계하며 OK 재검색은 파일 경로로 선택을 복원한다.
- `image_probe.*`가 BMP/PNG/JPEG/GIF/BIN의 서명·핵심 헤더·끝 구조를 검사한다. LVGL decoder
  확인을 통과한 파일은 이름, 순번, 형식, 치수와 크기를 표시하고 긴 이름은 한 줄 말줄임한다.
- PC 임시 저장소 fixture로 빈 상태, `Image2`/`Image10` 자연 정렬, 손상 PNG, 100자 이상
  파일명과 선택 보존 새로고침을 smoke에 추가했다. 네 상태 preview를 480x320에서 확인해
  이미지·오류 문구·하단 정보·말줄임이 겹치지 않음을 확인했다.
- 호스트 CTest 14/14, 깨끗한 PC Debug 전체 빌드와 smoke가 통과했다. 추적
  `sim/build/pedal_sim.exe`를 같은 소스로 갱신했고 사용자 `sim_nvs.bin` SHA-256은
  `0766BA4E680CBEF698728E0834333D8CC802A8820A1D62C08A5C3DE9AF0BE26D`로 유지됐다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 `-Werror=all` 빌드가 통과했다. 이미지는
  각각 `0xec280`(1MiB 앱 파티션 8% 여유), `0xe8c60`(9% 여유)다.
- 본체 플래시와 실제 FAT32 SD 카드의 mount·이미지 전환·LCD 공유 SPI 실기 검증은 수행하지
  않았다. 외부 9V와 USB 장치 상태도 변경하지 않았다.

### 2026-08-06 D6 외부 Game Boy 코어

- MIT Peanut-GB 커밋 `8e656982f08663785794b84823d3e27f856fdb7f`를 원형 그대로
  포함하고, 공통 `game_core`/`game_runtime` 경계에서 PC와 GG가 같은 코어를 빌드한다.
  첫 구성은 DMG `.gb`만 지원하며 외부 게임 오디오는 PC와 GG 모두 무음이다.
- ROM 확장자뿐 아니라 선언 크기, 헤더 체크섬, DMG 호환 플래그와 Peanut-GB가 지원하는
  카트리지 형식을 검사한다. 통과한 파일만 Game 로비 타일에 표시하고, save RAM은 앱을
  떠날 때 ROM 옆의 같은 이름 `.sav`로 기록한다. ROM 파일은 저장소에 포함하지 않았다.
- 외부 플레이어는 160x144 프레임을 2배 정수 확대해 320x288로 표시한다. 코어 진행은
  59.7Hz, 화면 발행은 30fps이며 최대 3프레임 catch-up으로 UI 정지를 제한한다.
  6키 GG는 방향키=D-pad, HOME 짧게=A/B/START/SELECT/BACK 선택, OK=선택 동작이다.
  PC는 같은 보조 조작과 함께 `Z/X/A/S` 직접 A/B/SELECT/START 입력을 제공한다.
- 호스트 CTest 15/15, 깨끗한 PC Debug 전체 빌드와 smoke가 통과했다. smoke는 합성 DMG
  ROM probe·프레임 진행·로비 복귀와 빈 타일 Cat Run을 검사한다. 추적
  `sim/build/pedal_sim.exe`도 같은 smoke를 통과했고 `sim_nvs.bin` SHA-256은 전후
  `0766BA4E680CBEF698728E0834333D8CC802A8820A1D62C08A5C3DE9AF0BE26D`로 유지됐다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 `-Werror=all` 빌드가 통과했다. 이미지는
  각각 `0xf29a0`(1MiB 앱 파티션 5% 여유), `0xef410`(7% 여유)이며 파티션 표는 바꾸지
  않았다.
- 본체 플래시와 실제 SD의 합법적으로 보유한 `.gb` 로딩, LCD 프레임 시간, 6키 조작,
  `.sav` 생성·재로드는 아직 실기 검증하지 않았다. 외부 9V와 장치 연결 상태는 변경하지
  않았다.

### 2026-08-06 D7 MIDI Monitor

- `midi_service.*`를 추가해 parser가 완성한 메시지의 최근 비클록 이력 16개, 전체·clock
  수, 최신 Program Change와 sequence를 seqlock snapshot으로 발행한다. Monitor capture
  중에는 기존 scene·CC·clock 매핑을 중복 실행하지 않고 앱을 나가면 복원한다.
- PC는 WinMM 입력 callback의 lock-free 큐를 시뮬레이터 메인 루프에서 `midi_feed()`로
  배출하고 짧은 메시지 출력을 지원한다. CLI는 `--list-midi`, `--midi-in`, `--midi-out`,
  `--no-midi`다. 현재 PC의 실제 열거 결과는 입력 0개, 출력 1개
  `Microsoft GS Wavetable Synth`였다.
- MIDI Monitor는 현재 메시지와 5개 이력, 누적·clock·drop 수를 표시한다. 좌우 채널 필터,
  OK pause/live, UP clear이며 clock은 이력 행을 채우지 않는다. 480x320 실제 SDL 창에서
  겹침과 보조 텍스트 대비를 확인했다.
- 새 MIDI service를 포함한 호스트 테스트, 깨끗한 PC Debug 빌드와 전체 smoke가 통과했다.
  smoke는 가상 MIDI로 Monitor 이력·필터·pause·clear·capture를 검증했다. 추적
  `sim/build/pedal_sim.exe`도 갱신했고
  `sim_nvs.bin` SHA-256은 `0766BA4E...E26D`로 유지됐다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 `-Werror=all` 빌드가 통과했다. 이미지는
  각각 `0xf5180`(1MiB 앱 파티션 4% 여유), `0xf1b40`(6% 여유)이며 파티션은 바꾸지 않았다.
  다음 대규모 앱 추가 전에 16MB flash용 앱 파티션 확장·NVS 보존 계획을 먼저 승인해야 한다.
- 현재 GG에는 MIDI 물리 단자와 UART/BLE 백엔드가 없어 플랫폼은 unavailable stub이다.
  이번 단계에서는 본체 플래시·물리 MIDI·실기 버튼 검증을 하지 않았고 외부 9V와 USB 연결
  상태도 변경하지 않았다.

### 2026-08-06 16MB 앱 파티션 확장 준비

- `partitions.csv`와 `sdkconfig.defaults`를 16MB 커스텀 표로 전환했다. 기존 NVS
  `0x9000`/`0x6000`, PHY `0xf000`/`0x1000`, factory 시작 `0x10000`은 유지한다.
  factory는 4MB, `otadata`는 `0x410000`/8KB, `ota_0`과 `ota_1`은 각각
  `0x420000`·`0x820000` 시작 4MB다. 뒤쪽 약 3.9MB는 할당하지 않았다.
- ESP-IDF 5.4.4 파티션 생성·역변환 검증과 기본/`INPUT_TRS_LADDER=0`
  `-Werror=all` 깨끗한 전체 빌드가 통과했다. 이미지는 각각 `0xff3a0`, `0xfbdb0`이고
  최소 4MB 앱 슬롯의 75%가 남는다. 호스트 CTest도 현재 목록 17/17을 통과했다.
- 2026-08-06 확인 시 Windows에 COM 및 Espressif/USB serial 장치가 하나도 열거되지 않아
  NVS 백업과 파티션 변경 플래시는 수행하지 않았다. 장치 재연결 뒤 NVS `0x9000`의
  `0x6000`바이트를 먼저 백업하고 전체 flash erase 없이 기본 변형을 플래시해야 한다.

### 2026-08-07 런처 오버플로 표시·Oscilloscope

- LIVE/STASH 행에서 화면 왼쪽이나 오른쪽에 가려진 앱이 있으면 해당 가장자리에 삼각형
  표시를 그린다. 현재 선택 행의 표시는 테마 강조색으로 선명하게, 다른 행은 낮은 불투명도로
  표시한다. PC 실제 창에서 첫 화면 오른쪽 표시와 스크롤 뒤 양방향 표시 위치를 확인했다.
- Setlist 앱과 CSV catalog, 저장소 media 종류, MIDI capture 경로, 호스트 테스트 및 관련
  문서 계약을 제거했다. 같은 레지스트리 위치에는 Oscilloscope를 등록해 앱 수는 10개를
  유지한다.
- Oscilloscope는 Core1이 발행하는 2,048-sample 파형 snapshot만 소비한다. 앱이 활성화된
  동안에만 256 sample마다 별도 double-buffer seqlock으로 발행하며 I2S/DSP 소유권은
  Core1에 그대로 둔다. 시간축 2/5/10/20ms, 세로 범위 ±0.10/0.20/0.50/1.00FS,
  상승 영점 trigger와 free-run fallback, OK Hold/Run을 구현했다.
- 깨끗한 호스트 테스트 15/15, 깨끗한 PC 빌드와 synthetic smoke가 통과했다. PC 실제 창에서
  런처 표시와 Oscilloscope의 480x320 배치·파형 표시를 확인했고 추적
  `sim/build/pedal_sim.exe`도 갱신했다.
- ESP-IDF 5.4.4 기본과 `INPUT_TRS_LADDER=0` 전체 `-Werror=all` 빌드가 통과했다. 최종 이미지는
  각각 `0xfe8c0`, `0xfb230`이며 4MB 앱 파티션에서 모두 75%가 남는다.
- COM4에서 실제 16MB flash를 확인하고 NVS `0x9000`/`0x6000`을
  `device_backups/nvs_COM4_20260807_pre_oscilloscope.bin`으로 보존했다. 백업은 24,576바이트,
  SHA-256 `9D75FAB9B16FF285039A8D4CF0D7BCD62D466840DE56B7E85780E9E5F3F109CA`다. 그 뒤 전체
  삭제 없이 bootloader·partition table·factory·otadata를 기록했고 각 write hash가 통과했다.
- 첫 부팅에서 Oscilloscope의 12KB 정적 파형 저장소 때문에 LVGL 내부 DMA 버퍼가 부족한
  것을 발견해 파형 저장소를 앱 진입 시 PSRAM에 할당하도록 바꿨다. LVGL 버퍼 자체를 PSRAM
  DMA로 옮기는 시도는 ST7796 SPI queue 오류와 watchdog을 일으켜 폐기했다. 최종본은 내부
  DMA 이중 버퍼를 유지하되 한 버퍼 높이를 40줄에서 30줄로 낮췄다.
- 최종 30초 부팅 로그에서 16MB 파티션, 8MB PSRAM 테스트, LVGL·TRS ladder 초기화와
  Sound Monitor 27~30fps를 확인했다. 이어 재부팅 없는 45초 로그에서 OK 입력이 반복 감지됐고
  SPI·메모리·watchdog 오류는 없었다. 런처 화살표와 Oscilloscope 파형·Hold/Run의 육안
  판정은 사용자 확인을 기다린다.

### 2026-08-10 남은 하드웨어 목표 회로 확정

- PCM1808은 분석 전용 ADC로 유지하고 재생/AUX/헤드폰은 Adafruit TLV320DAC3100
  Product 6309 모듈로 확정했다. G9/G18 클럭을 PCM1808과 공유하고 G40 I2S data,
  G41/G42 I2C, G7 reset을 사용한다. AUX L/R은 2.2uF+10k를 거쳐 AIN1/AIN2로 들어가고
  앱 음원과 함께 온보드 헤드폰 잭으로만 출력된다.
- 물리 MIDI는 절연형 3.5mm TRS-A 두 단자, 6N138 MIDI IN(G5), 5V SN74AHCT14 두 gate
  MIDI OUT(G6)으로 확정했다. MIDI IN Sleeve에는 회로 GND DC 경로를 만들지 않는다.
- 메인 기타 뮤트는 J201을 폐기하고 AQY221R2S 저용량 normally-open PhotoMOS를 출력
  Tip-Sleeve에 병렬로 두는 방식으로 확정했다. ESP32-S3 strapping G3 대신 G39를 쓰며
  `app_main.c` 핀도 G39로 변경했다. 전원 OFF에는 하드와이어 Tip 직결만 남는다.
- USB 단독 사용 시 MP1584 출력 역급전을 막기 위해 D1은 3A 1N5822, buck 출력 격리는
  2.5A Adafruit LM66200 Product 5830을 쓰도록 목표 전원 회로를 보강했다. MP1584 `OUT+`를
  4.9~5.1V로 조정한다.
- 사용자용 구매표 `hardware/PURCHASE_LIST.md`를 추가하고 `ASSEMBLY.md`를 위 목표 회로
  전체 배선표로 교체했다. `NETLIST_SPEC`, 제품 정의와 로드맵도 같은 선택으로 맞췄다.
- 자동 듀얼레인지 입력은 임시 TL072+LINE/INST를 다시 만들지 않고 OPA2192 목표 회로로
  진행한다. 고임피던스/pF 구간은 납땜 기판에서 만들고 sweep 교정한다.
- G39 변경 뒤 ESP-IDF 기본/`INPUT_TRS_LADDER=0` `-Werror` 빌드가 통과했다. 바이너리는
  각각 `0xfea90`/`0xfb4c0` bytes로 4MB 앱 파티션에 75%가 남으며 호스트 테스트 15개도
  모두 통과했다. 앱/UI/공유 DSP 변경이 없어 PC 시뮬레이터는 다시 빌드하지 않았다.
- 이번 기록은 설계와 문서 확정이다. 사용자가 실물 변경을 보고하지 않았으므로
  `hardware/AS_BUILT_WIRING.md`는 갱신하지 않았다. 부품 구매·배선·플래시·실기 검증도
  수행하지 않았으며 외부 9V/USB 상태를 변경하지 않았다.

## 7. 다음 작업

1. 현재 플래시본의 런처 화살표, Oscilloscope 파형·시간축·감도·Hold/Run을 사용자 육안으로
   확인한다.
2. 소프트웨어 우선순위는 시스템 오디오·마이크·미디어 폴더·출력·MIDI 장치 선택 UI다.
3. 다음 실기 차례에는 USB 단독에서 실제 FAT32 SD의 DMG `.gb` 화면·프레임·6키 입력과
   `.sav` 생성·재로드를 확인한다.
4. `hardware/PURCHASE_LIST.md`의 부품을 조달하고 `ASSEMBLY.md`에 따라 코덱/AUX/헤드폰,
   MIDI, PhotoMOS 뮤트와 OPA2192 분석 기판을 무전원 상태에서 조립한다.
5. 실물 변경 때마다 사용자가 `hardware/AS_BUILT_WIRING.md`를 갱신하고 Codex가
   `NETLIST_SPEC`과 연속성·레일 단락을 대조한다.
6. 하드웨어를 통과하면 ESP TLV320DAC3100 I2S/I2C playback backend와 UART MIDI backend를
   구현한 뒤 USB 단독 디지털 시험, 외부 9V 단독 오디오 교정 순서로 검증한다.
7. 배선 완료된 SD 모듈은 USB 단독에서 FAT32 Gallery와 LCD 공유 SPI를 실기 검증한다.
