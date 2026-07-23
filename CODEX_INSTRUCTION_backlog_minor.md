# Codex 지시서(소형 2건) — bars/reactive 렌더러 최적화 · TRS 6키 저항 래더 입력

> 두 건은 **독립적**이다. 순서 무관, 각각 별도 커밋 권장.
> 공통 불변조건: `renderer_t`/`gadget_app_t` 인터페이스·심볼명 유지, 오디오 파이프라인 무변경,
> ESP 빌드 `-Werror` 통과, 호스트 테스트 3/3 유지, PC 시뮬레이터 빌드 무회귀.

---

# 건① bars/reactive 렌더러 최적화 (curve 패턴의 오브젝트판)

## 배경
`renderer_curve.c`는 이미 최적화됐다. 나머지 둘은 **LVGL 오브젝트 기반**이라 접근이 다르다.
현재 두 렌더러 모두 매 프레임 **무조건** `lv_obj_set_*` / `lv_obj_set_style_*`를 호출한다.
LVGL은 set 호출 시 값이 같아도 내부적으로 invalidate → 불필요한 flush가 발생한다.
**값이 바뀐 경우에만 set을 호출**하도록 이전 상태를 캐시하는 것이 이 작업의 전부다.

## ①-A `renderer_bars.c`
- 현재 `b_update()`는 32개 막대 각각에 대해 매 프레임 다음을 무조건 호출한다:
  `lv_obj_set_size(bar)`, `lv_obj_set_y(bar)`, `lv_obj_set_style_bg_color(bar)`,
  그리고 피크의 `lv_obj_set_y(peak)` + `HIDDEN` 플래그 add/remove.
- 막대별 **이전 상태 캐시**를 정적 배열로 둔다: `prev_h[NB]`, `prev_color[NB]`,
  `prev_peak_y[NB]`, `prev_peak_hidden[NB]`.
- 각 항목은 **변화가 있을 때만** 대응 set 호출. 특히:
  - 높이 `h`가 같으면 `lv_obj_set_size` + `lv_obj_set_y` 둘 다 스킵(둘은 같은 `h`에서 파생).
  - 색은 `levelc(v)`의 **결과 uint32_t를 비교**한다(임계값 기반이라 대부분 프레임에서 불변).
  - 피크는 y좌표와 hidden 상태를 각각 따로 비교. `lv_obj_add_flag`/`remove_flag`도
    상태가 실제로 바뀔 때만 호출.
- `b_create()`에서 캐시를 **초기 오브젝트 상태와 일치하도록 초기화**한다
  (`prev_h=1`, `prev_color=t->lo`, `prev_peak_hidden=true`, `prev_peak_y=PY+PH`).
  ⚠ 캐시를 0/미초기화로 두면 첫 프레임이 스킵되어 화면이 안 그려지는 버그가 난다.
- `b_destroy()`에서 캐시 무효화(다음 `b_create`가 깨끗하게 시작되도록).

## ①-B `renderer_reactive.c`
- `r_update()`가 매 프레임 무조건 호출하는 것: body의 size/radius/pos/bg_color,
  eyeL/eyeR의 size/radius/pos, mouth의 size/radius/pos.
- 오브젝트별로 이전 `(w,h,x,y,radius,color)`를 캐시하고 **변경분만** set.
- 이 렌더러는 `s_onset` 감쇠 때문에 값이 자주 바뀌지만, **정지 신호에서는 수 프레임 내
  모든 값이 수렴**하므로 그 구간에서 flush가 0으로 떨어져야 한다.
- `r_create()`에서 캐시를 "불가능한 값"(예: `-1`)으로 초기화해 **첫 프레임은 반드시 그리도록** 한다.
  (bars와 달리 create에서 초기 geometry를 세팅하지 않으므로 이쪽이 맞다.)
- **특징 추출 로직(level/centroid/onset 계산)은 절대 변경 금지.** 시각 거동이 바뀌면 안 된다.

## 건① 불변조건
- `RENDERER_BARS` / `RENDERER_REACTIVE` 심볼과 `"bars"` / `"reactive"` 이름 문자열 유지.
- `renderer_t` 3함수 시그니처 유지. 공개 헤더 변경 0.
- 오디오·테마·앱 레이어 파일 수정 0. 두 `.c` 파일만 건드린다.
- 시각적 무회귀: 같은 입력에서 픽셀 결과가 동일해야 한다(캐시는 순수 최적화).

## 건① 완료 판정
- 정지(무신호) 상태에서 두 렌더러 모두 set 호출이 0에 수렴 → flush≈0.
- **PC 시뮬레이터로 육안 확인 가능**: `sim/`이 `renderer_bars.c`/`renderer_reactive.c`를
  그대로 컴파일하므로, 플래시 전에 sim에서 Monitor 화면을 실오디오로 돌려
  최적화 전/후 시각 차이가 없음을 확인할 것.
- ESP 빌드 `-Werror` 통과.

---

# 건② TRS 6키 저항 래더 입력 (하드웨어 완성됨 — 즉시 투입 가능)

## 하드웨어 현황 (2026-07 실측 완료)
```
Ring(+3V3) ─[Rtop 4.7k]─┬─→ Tip ──→ ESP32-S3 G4 (ADC1_CH3)
                         │
   UP    ─ 직결(0Ω)  ─ Sleeve(GND)
   DOWN  ─ [150Ω]   ─ Sleeve
   LEFT  ─ [470Ω]   ─ Sleeve
   RIGHT ─ [1k]     ─ Sleeve
   OK    ─ [2k]     ─ Sleeve
   HOME  ─ [10k]    ─ Sleeve
```
- **기존 6개 GPIO 택트 스위치(G4/G5/G6/G7/G16/G15)는 물리적으로 모두 제거됨.**
- FOOTSW(G17)는 GPIO 그대로 유지.
- DMM 실측(무입력 3.21V 기준)에서 6키 전부 이론값 대비 **오차 1% 이내**,
  `RIGHT+OK` 동시 입력 405mV(이론 399mV) 확인 → 계산 모델과 실물 일치 검증됨.

## ②-A 컴파일 스위치
`app_main.c` 상단에 `#define INPUT_TRS_LADDER 1` 을 추가한다.
- **기본값은 1**(래더 모드). 이유: GPIO 버튼이 물리적으로 제거되어 0 빌드로는 손 입력이 불가능하다.
- `0`으로 빌드해도 **컴파일은 성공**해야 한다(구 GPIO 경로 롤백/참조용). 기능 검증은 불필요.
- `1`일 때: `BTN_*_IO` 6핀을 GPIO 입력으로 설정하지 않는다.
  `input_cfg.pin_bit_mask`에서 6핀을 빼고 **FOOTSW_IO만** 남긴다. G4는 ADC가 점유한다.

## ②-B 판정 방식 — 비율(ratiometric) + 창/데드존
**⚠ 이 두 가지는 타협 불가다. 어기면 오작동한다.**

1. **최근접(nearest) 판정 금지.** 반드시 *창(window) + 데드존* 방식.
   경계 중간값으로 나누면 `RIGHT+OK` 동시 입력(비율 0.124)이 **LEFT로 오인식**된다.
   창 밖의 모든 값은 **무입력**으로 처리해야 한다.
2. **절대 mV가 아닌 비율로 판정.** `ratio = reading / idle_reference`.
   공급 전압·ADC 기준전압 드리프트가 통째로 상쇄된다.
   `idle_reference`는 무입력 상태(비율 0.85 초과)의 읽기값을 느린 시정수로 자동 추적한다
   (예: `idle_ref = idle_ref*0.99 + reading*0.01`, 초기값은 첫 무입력 읽기).

### 판정표 (실측 기반, 상단 `#define` 테이블로)
| 키 | 비율 중심 | 창 (±) | 참고: @3.21V 환산 |
|---|---|---|---|
| UP | 0.00000 | 0.0070 | 0 ~ 23 mV |
| DOWN | 0.03093 | 0.0081 | 73 ~ 125 mV |
| LEFT | 0.09091 | 0.0093 | 262 ~ 322 mV |
| RIGHT | 0.17544 | 0.0296 | 468 ~ 658 mV |
| OK | 0.29851 | 0.0431 | 820 ~ 1096 mV |
| HOME | 0.68027 | 0.1119 | 1824 ~ 2543 mV |
| 무입력 | ≥ 0.85 | — | ≥ 2728 mV |

- 창 폭은 이론 최대 허용치의 70%로 잡아 데드존을 확보한 값이다. 57개 전 조합 검증에서
  오인식 0건. 각 이론값 옆에 유래(저항값)를 주석으로 남길 것.

## ②-C 동시 입력 정책 (물리 근거)
병렬 저항이므로 **키를 추가하면 전압은 반드시 내려간다**. 이 성질을 그대로 규칙화한다:

- 현재 latch된 키 없음 → 유효 창에 들어온 키를 latch.
- latch된 키 있음:
  - **무입력 창(≥0.85)** → 해제. *(모든 키를 뗐다는 유일한 확증)*
  - 같은 키 창 → 유지.
  - **더 높은 비율**의 다른 키 창 → 기존 키를 뗀 것이므로 해제 후 새 키 latch.
  - **더 낮은 비율**의 다른 키 창 / 데드존 → 키가 추가된 모호 상태. **무시하고 현재 키 유지.**

## ②-D 기존 입력 로직 재사용 (핵심 설계)
`input_button_t` + `input_button_update()`의 디바운스/홀드/오토리피트를 **그대로 재사용**한다.
새 이벤트 발행 로직을 따로 만들지 말 것.

방법: `input_button_update()`가 `gpio_get_level(button->pin)`으로 얻던 **raw level의 출처만 교체**한다.
- `input_button_t`에 소스 구분 필드를 추가하거나(예: `bool from_ladder`),
  raw level 획득을 작은 함수 `input_read_raw(button)`로 분리한다.
- 래더 모드에서 6키의 raw level은 ADC 디코드 결과로 **합성**한다:
  latch된 키 = `0`(눌림, active-low 규약 유지), 나머지 5키 = `1`.
- FOOTSW는 항상 `gpio_get_level(FOOTSW_IO)`.
- 결과적으로 `INPUT_DEBOUNCE_MS(30)`·`INPUT_HOLD_MS(500)`·`INPUT_REPEAT_DELAY_MS(400)`·
  `INPUT_REPEAT_RATE_MS(120)`가 전부 자동 적용된다. **상수 변경 금지.**
- 30ms 디바운스가 키 간 롤오버 시 과도 상태를 자연스럽게 걸러준다.

## ②-E ADC 설정
- **ESP-IDF v5.4.4의 `esp_adc/adc_oneshot.h` 신규 드라이버 사용.** 레거시 `driver/adc.h` 금지.
- `ADC_UNIT_1`, `ADC_CHANNEL_3`(=GPIO4), `ADC_ATTEN_DB_12`, `ADC_BITWIDTH_DEFAULT`.
  (IDF 5.x에서 `ADC_ATTEN_DB_11`은 deprecated → `ADC_ATTEN_DB_12`)
- **오버샘플링 32회 평균.** UP/DOWN/LEFT 창이 좁아(비율 0.007~0.009) 노이즈 억제가 필수다.
- `adc_cali_curve_fitting`으로 raw→mV 변환을 시도하되, **실패해도 동작해야 한다**
  (비율 판정이므로 raw count 비율로도 성립). 캘리브레이션 유무를 로그 1줄로 남길 것.
- ADC 읽기는 `input_task`의 기존 `INPUT_POLL_MS(10)` 루프 안에서 수행.
  블로킹 없이 원샷으로 끝나야 한다.

## ②-F 튜닝 로그
`#define INPUT_TRS_LOG 1` 매크로로 on/off 하는 **1초 주기 로그**:
```
I (12345) input: ladder raw=1234 mV=1002 ratio=0.0312 -> DOWN (idle_ref=3210mV)
```
- raw count, mV(캘리 가능 시), 비율, 판정 결과, 현재 idle_ref를 모두 출력.
- 기본값 1로 두고, 튜닝 완료 후 태윤이 0으로 내린다.

## 건② 불변조건
- `ui_event_t` enum(9종)·`EV_*` 이름 변경 0. 이벤트 발행 경로(`dispatch_input_event`) 재사용.
- 디바운스/홀드/리피트 **상수 및 로직 변경 0**.
- FOOTSW(G17) 거동 무변경. 뮤트(G3)·I2S·LCD 핀 무관.
- UI/앱/렌더러 레이어 수정 0. `app_main.c` + (필요시) 신규 파일 1개까지.
- PC 시뮬레이터 무영향(sim은 SDL 키보드 경로를 그대로 쓴다).
- `INPUT_TRS_LADDER=0` 빌드도 컴파일 성공.

## 건② 완료 판정
1. 6키 각각 정확 인식, 오인식 0. 방향키 오토리피트·HOME 롱프레스(런처 복귀) 정상.
2. `RIGHT+OK` 동시 입력 시 **아무 이벤트도 발생하지 않음**(데드존). LEFT가 뜨면 실패.
3. 키를 누른 채 다른 키 추가 → 처음 키가 유지되고 엉뚱한 키가 발생하지 않음.
4. TRS 케이블 분리 시 무입력(이벤트 없음), 재연결 시 정상 복귀.
5. FOOTSW 단/장 누름 정상.
6. ESP 빌드 `-Werror`, 호스트 테스트 3/3, sim 빌드 무회귀.

## ⚠ 검증 게이트 — ADC 저전압 구간 (반드시 로그로 확인)
ESP32-S3 ADC는 **0V 근처에서 오프셋·비선형이 있을 수 있다**. UP은 0V, DOWN은 약 100mV로
가장 가까운 쌍이므로 여기가 유일한 위험 지점이다.

②-F 로그로 **UP과 DOWN의 실제 ADC 읽기값 차이**를 확인하고 보고할 것:
- 차이가 **40mV 이상**이면 그대로 진행.
- 차이가 40mV 미만이거나 UP이 0이 아닌 큰 값으로 읽히면 → **진행 중단하고 실측값을 보고**할 것.
  저항값 재튜닝(태윤 보유: 10·100·150·220·330·470·510·1k·2k·4.7k·10k·100k·1M·10M)이 필요하다.

DMM 실측은 회로가 정상임을 확인한 것이고, **최종 창 확정은 ADC 자신이 읽은 값 기준**이어야 한다.
로그값이 위 판정표와 크게 다르면 표를 ADC 실측 중심으로 옮겨 잡는다.

---

## 산출물
- 건①: `renderer_bars.c` / `renderer_reactive.c` diff, 캐시 초기화 처리 설명.
- 건②: `app_main.c` diff(+신규 파일 있으면 함께), ADC 초기화 코드, 판정표 위치.
- **문서 동기화(SSOT 필수)**: `hardware/NETLIST_SPEC.md`와 `ASSEMBLY.md`의 G4 항목을
  "UP 버튼" → "TRS 래더 ADC 입력(ADC1_CH3), Rtop 4.7k, 키 저항 0/150/470/1k/2k/10k"로 갱신.
  제거된 5개 GPIO 버튼(G5/G6/G7/G16/G15)은 **미사용(Phase 2 예비)** 로 표기.
- ②-F 로그 실측값(6키 + 무입력 7개) 보고 — 특히 UP/DOWN 분리 여부.
