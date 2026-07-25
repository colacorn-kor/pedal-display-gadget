# Codex 지시서(핫픽스) — input 태스크 워치독 리셋 수정

> 단일 목적. 부팅·240MHz·PSRAM·화면 초기화는 모두 성공했으나, `input` 태스크가 CPU0에서
> IDLE0를 굶겨 **5초마다 워치독 리셋**이 발생한다(화면이 정적 노이즈로 멈추고 버튼 무반응).
> 이 지시서 범위 밖은 건드리지 말 것.

## 확인된 원인 (실기 로그 + 계산)
```
E task_wdt: IDLE0 (CPU 0)   /  CPU 0: input
adc_oneshot_read → input_ladder_read_average (app_main.c:534) → input_task (:818)
```
- CPU0 태스크 배치: `display`(prio 4), **`input`(prio 5)**, LVGL(`task_affinity=0`), IDLE0.
- `input`이 `display`보다 우선순위가 높다. `input_ladder_read_average`가 매 폴에서
  `adc_oneshot_read`를 **32회 블로킹 연속 호출**(크리티컬 섹션 반복 진입)하고, 폴 주기가
  5ms로 짧아, `display`와 IDLE0가 실행 기회를 얻지 못한다 → IDLE0 5초 미실행 → 워치독.
- 화면 정적 노이즈 = LVGL(CPU0)이 밀려 첫 프레임 미완성. 버튼 무반응 = 반복 리셋.

## 수정 (3개, 모두 `app_main.c`)

### A. ADC 읽기: 폴당 32회 → **폴당 1회 + 32탭 이동평균**
`input_ladder_read_average`가 루프에서 32회를 읽는 구조를 제거하고, **폴마다 1회만** 읽어
링버퍼 이동평균으로 평활한다.
- 정적 배열 `static int s_adc_ring[INPUT_TRS_ADC_SAMPLES]` + 인덱스 + 합계 유지.
- 매 폴: 1회 `adc_oneshot_read` → 링버퍼에 삽입 → 이동합/`INPUT_TRS_ADC_SAMPLES`가 평균.
- 워밍업: 첫 `INPUT_TRS_ADC_SAMPLES` 폴 동안은 채워진 개수로 나눈다(부팅 직후 idle_ref 오염 방지).
- 결과: CPU0 점유율 약 26% → 1% 미만. 평활 품질은 32샘플로 동일, 지연은 폴에 분산되어 감소.
- `INPUT_TRS_ADC_SAMPLES`(32) 상수는 유지(이제 이동평균 탭 수 의미).

### B. `input` 태스크 우선순위 5 → **3**
`xTaskCreatePinnedToCore(input_task, "input", 4096, NULL, 5, NULL, 0)`의 우선순위를 **3**으로
낮춘다. `display`(4)보다 낮아져 렌더링·IDLE0가 굶지 않는다. 입력은 지연 예산이 커서
우선순위 강등의 체감 영향이 없다. **코어(0)·스택은 유지.**

### C. 폴 주기 5ms → **10ms** 복귀
`#define INPUT_POLL_MS 5` → **10**. 이동평균(A)이면 10ms로도 충분히 반응하며, 안정성
카운터 3회와 합쳐 총 지연 약 55ms로 수용 범위. 5ms는 이 하드웨어에 과부하였다.
- ⚠ `INPUT_HOLD_MS`/`REPEAT_DELAY`/`REPEAT_RATE`는 폴 단위 누적(`+= INPUT_POLL_MS`)으로
  계산되므로, `INPUT_POLL_MS` 변경 후에도 **실시간 값이 동일하게 유지되는지** 확인
  (누적 로직이 상수를 참조하면 자동 정합, 하드코딩이면 수정).
- 건④의 버튼별 `debounce_ms`(래더 10ms / FOOTSW 30ms)는 유지.

## 불변조건
- UI/앱/렌더러·오디오 파이프라인·`tuner.c`·판정표·래치 로직 변경 0.
- ADC 초기화(`input_ladder_init`)·캘리브레이션 경로 변경 0(읽기 방식만 A로 교체).
- `audio_task`(CPU1, prio 6)·`display_task`(CPU0, prio 4) 배치 변경 0.
- `-Werror` 통과, 호스트 테스트 3/3, sim 빌드 무회귀, `INPUT_TRS_LADDER=0` 컴파일 성공.

## 완료 판정
1. 부팅 후 **워치독 리셋 없음**(5분 이상 관찰).
2. 화면에 정상 UI 표시(정적 노이즈 아님).
3. 6키 입력·방향키 이동·HOME 롱프레스·FOOTSW 동작.
4. `INPUT_TRS_LOG` 라인이 1초 주기로 계속 출력되고 리셋으로 끊기지 않음.
5. 부팅 로그 `cpu freq: 240000000 Hz` 유지.

## 산출물
- `app_main.c` diff(A 이동평균 / B 우선순위 / C 폴주기).
- 변경 후 input 태스크 CPU0 점유율 재계산.
- `INPUT_POLL_MS` 변경이 hold/repeat 실시간 값에 영향 없음을 확인한 메모.
