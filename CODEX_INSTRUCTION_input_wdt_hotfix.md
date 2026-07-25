# Codex 지시서(핫픽스 v2) - input 태스크 워치독 리셋 수정

> 상태: 자동 검증과 310초 WDT 무발생 로그 통과, 사용자 UI·입력 확인 대기.
> 이 파일은 실기 완료 후 삭제한다. 현재 결과는 `LAB_STATE.md`에 기록한다.

## 목적

부팅 후 약 5초마다 발생하는 CPU0 task watchdog 리셋을 제거한다. UI, 앱, 오디오,
래더 판정과 사용자 입력 타이밍은 바꾸지 않는다.

## 인수 감사에서 확인한 직접 원인

```text
E task_wdt: IDLE0 (CPU 0)
CPU 0: input
adc_oneshot_read -> input_ladder_read_average -> input_task
```

- `CONFIG_FREERTOS_HZ=100`이므로 1 tick은 10ms다.
- `INPUT_POLL_MS=5`에서 `pdMS_TO_TICKS(5)`는 0이다.
- 우선순위 5인 input 태스크가 `vTaskDelay(0)`으로 반복되어 CPU0의 display(4), LVGL,
  IDLE0를 굶긴다.
- ADC 32회 블로킹 버스트가 각 반복의 점유 시간을 늘리지만, 0 tick 지연이 직접 원인이다.

## 원 지시서에서 폐기한 변경

10ms마다 ADC 한 샘플을 넣는 32탭 이동평균은 적용하지 않는다. 시간 창이 320ms가 되어
UP 입력이 HOME(150~170ms), OK, RIGHT, LEFT 판정 창을 차례로 통과한다. HOME이 3회
연속 검출되면 기존 래치 정책상 UP까지 내려가지 못하고 HOME에 고착될 수 있다.

평활과 래더 지연 특성을 보존하기 위해 기존의 짧은 32샘플 버스트 평균을 유지한다.

## 코드 수정

모두 `app_main.c`에 한정한다.

1. `INPUT_POLL_MS`를 5ms에서 10ms로 복귀한다.
2. `pdMS_TO_TICKS(INPUT_POLL_MS)`가 0이어도 최소 1 tick을 사용하도록 방어한다.
3. input 태스크 우선순위를 5에서 3으로 낮춘다. 코어0과 4096 스택은 유지한다.
4. ADC 32샘플 평균, 초기화, 캘리브레이션, 판정표, 래치 로직은 변경하지 않는다.

`INPUT_HOLD_MS=500`, `INPUT_REPEAT_DELAY_MS=400`, `INPUT_REPEAT_RATE_MS=120`은 각각
10ms의 50회, 40회, 12회 누적으로 실시간 값이 그대로 유지된다. 래더 10ms와 FOOTSW
30ms 디바운스도 유지한다.

## 불변조건

- UI, 앱, 렌더러, 오디오 파이프라인, `tuner.c`, 판정표, 래치 로직 변경 0
- `input_button_read_raw()` 캡슐화 유지
- audio 태스크 Core1/prio6, display 태스크 Core0/prio4 유지
- `-Werror`, 호스트 테스트 3/3, 시뮬레이터 빌드 통과
- `INPUT_TRS_LADDER=0` 컴파일 통과

## 완료 판정

1. 부팅 로그에 `cpu freq: 240000000 Hz`
2. 정상 UI 표시
3. 최소 5분간 task watchdog 리셋 없음
4. ladder 로그가 1초 주기로 끊기지 않음
5. 6키, 방향키 반복, HOME 짧게/길게, FOOTSW 짧게/길게 동작
