# LAB_STATE.md - 현재 장치 및 실기 상태

> 갱신일: 2026-07-27
> 이 문서는 마지막 플래시와 현재 실험 상태의 SSOT다.

## 1. 기준 상태

- 플래시 이미지 기준 커밋: `052f2876` (`Record Color Mode hardware verification`)
- 마지막 실기 펌웨어: 위 커밋의 dirty 작업 트리에서 빌드한 Curve/Reference 개발본
- 현재 펌웨어 상태: 자동 빌드·플래시·25초 부팅 로그 통과, Curve/Reference 사용자
  화면·버튼 실기 확인 통과
- 마지막 확인 포트: COM4
- 전원 전제: 사용자가 별도로 알리지 않는 한 외부 9V는 분리, USB만 연결된 상태
- 등록 앱: Sound Monitor, Images, Tuner, Bounce, dB Meter 총 5개
- 조립 상태: 사용자 확인 기준 `ASSEMBLY.md` 완료
- 미장착: SD 카드 모듈, 뮤트 회로
- Ring 100Ω / Tip 220Ω: `ASSEMBLY.md` 완료 범위에 포함되어 장착됨
- 오디오 입력 프론트엔드: 조립됨, 외부 9V 미연결 상태라 동작 미검증
- 목표 자동 듀얼레인지 회로는 문서만 확정했으며 현재 브레드보드는 구형 SPDT 회로 그대로

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
| 깨끗한 ESP-IDF 전체 빌드 / `-Werror` | 통과 (`pedal_display.bin` 0xd2250 bytes, 18% 여유) |
| `INPUT_TRS_LADDER=0` 컴파일 | 통과 (`0xcebd0` bytes, 19% 여유) |
| `AUDIO_DUAL_RANGE=1` 컴파일 | 통과 (`0xd25d0` bytes, 18% 여유), 현재 장치에는 미플래시 |
| 호스트 검증 5/5 | 통과 (CTest 4개: MIDI, tuner, audio level, autorange + FFT normalization) |
| PC 시뮬레이터 | 깨끗한 빌드와 전체 결정론적 smoke 통과 |
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
| Curve/Reference 실기 | tilt·단순화·FLAT 표시와 짧은 HOME/FOOTSW 모두 통과 |

## 6. PC 시뮬레이터 자동 확인

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
  읽는다. Step 5B 전에는 이 기본값을 유지한다.
- `AUDIO_DUAL_RANGE=1` 변형은 32-bit stereo I2S의 left=HOT, right=SENSITIVE를
  동시에 읽고 `audio_autorange`가 두 경로를 고정 GG 입력 스케일로 환산한다.
- SENSITIVE ADC peak 0.82에서 HOT으로 전환하고, 0.45 아래가 500ms 지속돼야
  SENSITIVE로 복귀한다. 클리핑 전환을 제외한 변경 블록에는 crossfade를 적용한다.
- dB Meter는 이 변형에서 수동 LINE/INST 대신 `AUTO SENSITIVE/HOT`과 clip 상태를
  표시하며, dBFS와 입력 Vrms·dBV·dBu를 명목 GG Input Full Scale로 계산한다.
- 호스트 autorange 테스트는 스케일 일치, 즉시 HOT 전환, hysteresis 복귀, clip 우회와
  비유한값 정리를 통과했다. CTest 4개와 별도 FFT normalization 검사까지 5/5 통과했다.
- ESP-IDF `-Werror` 기본 `0xd2250`, 래더 비활성 `0xcebd0`, 듀얼레인지 `0xd25d0`가
  모두 통과했고 PC 시뮬레이터 전체 smoke도 통과했다.
- 현재 하드웨어는 VINR SENSITIVE가 준비되지 않았으므로 듀얼레인지 이미지는 플래시하지
  않았다. 실제 두 채널 순서·overlap 일치·전환 연속성·I2S overflow는 Step 5B 뒤 검증한다.

## 7. 다음 작업

1. 외부 9V 오디오 검증은 USB를 분리한 별도 안전 절차에서 수행한다.
2. 현행 LINE/INST 회로의 1kHz gain·noise·clip 기준을 측정한다.
3. Step 5B 부품을 준비한 뒤 HOT/SENSITIVE 회로로 개조하고 듀얼레인지 변형을 플래시한다.
4. 범위별 1kHz와 20Hz~20kHz sweep으로 교정값과 GG Input Full Scale을 확정한다.
5. 뮤트 회로가 없으므로 물리 출력 뮤트는 별도 회로 장착 전까지 검증하지 않는다.
