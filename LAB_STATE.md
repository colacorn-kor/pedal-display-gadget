# LAB_STATE.md - 현재 장치 및 실기 상태

> 갱신일: 2026-07-26
> 이 문서는 마지막 플래시와 현재 실험 상태의 SSOT다.

## 1. 기준 상태

- 저장소 인수 기준: `166141c` (`CLAUDE_HANDOFF.md` 추가)
- 마지막 실기 펌웨어 기준: 이 변경의 input WDT 핫픽스 로컬 빌드
- 마지막 확인 포트: COM4
- 등록 앱: Sound Monitor, Images, Tuner, Bounce 총 4개
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

### TRS 입력 1차 묶음 실측

35초 동안 ladder 로그 35개를 수집했고 WDT, panic, reset, I2S 오류는 모두 0회였다.

| 상태 | 표본 수 | 최소 mV | 최대 mV | ratio 범위 |
|---|---:|---:|---:|---:|
| IDLE | 11 | 3208 | 3210 | 0.9995~1.0001 |
| DOWN | 5 | 136 | 165 | 0.0424~0.0514 |
| LEFT | 5 | 307 | 313 | 0.0956~0.0975 |
| RIGHT | 4 | 548 | 567 | 0.1707~0.1767 |
| OK | 4 | 1045 | 1062 | 0.3256~0.3309 |
| HOME | 6 | 1664 | 1693 | 0.5184~0.5275 |

모든 표본이 현재 판정창 안에서 올바른 키로 분류됐다. DOWN 최대 165mV와 LEFT 최소
307mV의 간격은 142mV다. UP은 기록 시작 직전 `latch=UP` 해제 흔적이 있어 인식 자체는
확인됐지만 눌린 상태의 1초 진단 표본이 없어 전압 범위와 UP/DOWN 간격 계산은 대기한다.

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
| 깨끗한 ESP-IDF 전체 빌드 / `-Werror` | 통과 (`pedal_display.bin` 0xc4a10 bytes) |
| `INPUT_TRS_LADDER=0` 컴파일 | 통과 |
| 호스트 테스트 3/3 | 통과 (MIDI, tuner, FFT normalization) |
| PC 시뮬레이터 | 빌드·창 실행 통과, 런처와 방향키 선택 이동 확인 |
| COM4 탐지 | 2026-07-26 확인 |
| USB 플래시 | 통과, 외부 9V 분리·USB 단독 상태 |
| 부팅 주파수 / PSRAM | 240MHz / 8MB 80MHz 확인 |
| WDT 5분 무발생 | 통과 (310초, ladder 310회, WDT/panic/reset 0) |
| 정상 UI 표시 | 통과 (Tuner, Sound Monitor 육안 확인) |
| FOOTSW 짧게 | 통과 (Tuner -> Sound Monitor) |
| 6키 ADC | DOWN/LEFT/RIGHT/OK/HOME 통과, UP 전압 표본 대기 |
| HOME 롱·FOOTSW 롱 | 사용자 화면 결과 확인 대기 |
| 신회로 7상태 ADC 로그 | UP을 제외한 6상태 수집 |
| USB-only 오디오 | TL072 무전원 부유 입력이라 기능 판정 제외 |

## 6. PC 시뮬레이터 자동 확인

- Codex가 실제 SDL 창을 실행해 런처 표시와 방향키 선택 이동을 확인했다.
- Windows 입력 자동화에서 방향키는 정상 전달됐지만 Enter/Space/Escape는 SDL 앱 이벤트로
  확정되지 않았다. 이는 실기 펌웨어 회귀로 판정하지 않는다.
- 공유 DSP의 MIDI, Tuner, FFT는 호스트 테스트 3/3으로 별도 검증됐다.
- 향후 물리 조작을 더 줄이려면 시뮬레이터에 GUI 키 주입과 무관한 결정론적 smoke CLI를
  추가한다.

## 7. 다음 실기 절차

1. 다음 메시지를 보낸 직후 UP을 10초간 계속 눌러 전압 표본을 확보한다.
2. HOME 길게와 FOOTSW 길게 뒤 실제 화면이 어떻게 바뀌었는지 사용자 관찰을 받는다.
3. UP/DOWN 간격을 계산하고 input 핫픽스 지시서를 종료한다.
4. 뮤트 회로가 없으므로 물리 출력 뮤트는 별도 회로 장착 전까지 검증하지 않는다.
5. 외부 9V 오디오 검증은 USB를 분리한 별도 안전 절차에서 수행한다.
