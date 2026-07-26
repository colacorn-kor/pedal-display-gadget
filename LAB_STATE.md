# LAB_STATE.md - 현재 장치 및 실기 상태

> 갱신일: 2026-07-26
> 이 문서는 마지막 플래시와 현재 실험 상태의 SSOT다.

## 1. 기준 상태

- 저장소 인수 기준: `166141c` (`CLAUDE_HANDOFF.md` 추가)
- 마지막 실기 펌웨어 기준: 12밴드/Circular/dB Meter 로컬 빌드
- 마지막 확인 포트: COM4
- 등록 앱: Sound Monitor, Images, Tuner, Bounce, dB Meter 총 5개
- 조립 상태: 사용자 확인 기준 `ASSEMBLY.md` 완료
- 미장착: SD 카드 모듈, 뮤트 회로
- Ring 100Ω / Tip 220Ω: `ASSEMBLY.md` 완료 범위에 포함되어 장착됨
- 오디오 입력 프론트엔드: 조립됨, 외부 9V 미연결 상태라 동작 미검증

## 2. 마지막 실기 결과

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
| 깨끗한 ESP-IDF 전체 빌드 / `-Werror` | 통과 (`pedal_display.bin` 0xc5a10 bytes, 23% 여유) |
| `INPUT_TRS_LADDER=0` 컴파일 | 통과 |
| 호스트 테스트 4/4 | 통과 (MIDI, tuner, audio level, FFT normalization) |
| PC 시뮬레이터 | 빌드·창 실행 통과, 결정론적 smoke 2/2 통과 |
| COM4 탐지 | 2026-07-26 확인 |
| USB 플래시 | 통과, 외부 9V 분리·USB 단독 상태 |
| 부팅 주파수 / PSRAM | 240MHz / 8MB 80MHz 확인 |
| WDT 5분 무발생 | 통과 (310초, ladder 310회, WDT/panic/reset 0) |
| 정상 UI 표시 | 통과 (Tuner, Sound Monitor 육안 확인) |
| FOOTSW 짧게 | 통과 (Tuner -> Sound Monitor) |
| 6키 ADC | 전부 통과, UP/DOWN 간격 110mV |
| HOME 롱·FOOTSW 롱 | 통과 (Launcher / Tuner) |
| 3행 런처·Settings·Reorder UI | 통과 (사용자 조작·커서 시인성 확인) |
| 신회로 7상태 ADC 로그 | 통과 |
| USB-only 오디오 | TL072 무전원 부유 입력이라 기능 판정 제외 |

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

## 7. 다음 작업

1. Basic 컨트롤러의 장시간 체감 평가는 실제 사용 중 이상이 있을 때 묶어서 수행한다.
2. 뮤트 회로가 없으므로 물리 출력 뮤트는 별도 회로 장착 전까지 검증하지 않는다.
3. 외부 9V 오디오 검증은 USB를 분리한 별도 안전 절차에서 수행한다.
