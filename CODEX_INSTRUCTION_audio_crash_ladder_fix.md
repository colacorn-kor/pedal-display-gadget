# Codex 지시서 — 오디오 크래시 · TRS 래치/판정표 · 반응성 · 문서 동기화

> 실기 로그로 확인된 문제와 확정된 하드웨어 변경을 반영한다.
> **건①(오디오 크래시)이 최우선** — 현재 튜너 앱을 켜면 워치독 리셋이 발생해 장치를 쓸 수 없다.
> 공통 불변조건: UI/앱/렌더러 레이어 수정 0, `-Werror` 통과, 호스트 테스트 3/3, sim 빌드 무회귀.

---

# 건① 오디오 태스크 폭주 → 워치독 리셋 (최우선)

## 증상 (실기 로그)
```
W (885153) app: I2S RX queue overflow count: 509      ← 90ms마다 계속 증가
E (890073) task_wdt: Task watchdog got triggered ... IDLE1 (CPU 1)
E (890073) task_wdt: CPU 1: audio
--- 0x4200ba2f: audio_task at app_main.c:292           ← ESP_LOGW 안에서 멈춤
--- 0x4200ce06: analyze at tuner.c:184                 ← 두 번째 백트레이스
```

## 원인 (측정·계산으로 확인됨)
- `analyze()`는 tau 5~268 × 내부 `WIN-tau` 루프 = **369,468회 반복** → 240MHz에서 **14~18.5ms**.
- 실행 주기는 `HOP 192 @ 8kHz` = **24ms** → **CPU 점유율 58~77%**.
- I2S DMA 버퍼는 `dma_desc_num=4 × DMA_FRAMES 256` = **21.3ms 분량**뿐.
  → `analyze()` 1회가 버퍼를 거의 소진 → 오버플로.
- 오버플로가 시작되면 `i2s_channel_read`가 **즉시 반환**(큐에 항상 데이터) →
  우선순위 6 `audio_task`가 블로킹 없이 회전 → **IDLE1 굶주림 → 워치독**.
- 오버플로 로그가 **카운트 변경마다** 발행된다. 115200baud에서 한 줄이 4ms 이상 태스크를
  붙잡아 상황을 악화시킨다(첫 백트레이스가 `uart_write` 안에서 잡힌 이유).

## ①-A 오버플로 로그 레이트 제한
`app_main.c` 292행 부근. 현재 `overflows != reported_overflows`이면 매번 로그한다.
- **최소 2000ms 간격**으로 제한. 마지막 로그 시각을 정적 변수로 유지.
- 억제 구간 정보가 사라지지 않도록 **누적/증가분 동시 출력**:
  `"I2S RX overflow: total=%u (+%u in last %ums)"`.
- 같은 함수의 `"I2S returned an invalid byte count"` 로그도 동일하게 2초 제한(동일 폭주 가능).

## ①-B DMA 버퍼 증설
- `chan_cfg.dma_desc_num = 4` → **8**.
- 버퍼 21.3ms → 42.6ms. `analyze()` 최악 18.5ms에 24ms 여유 확보.
- 추가 메모리 8 × 256 × 4B = 8KB(내부 RAM). 허용 범위.

## ①-C `analyze()` 실행 빈도 완화
- `tuner.c`의 `#define HOP 192` → **384**.
- analyze 주기 24ms → 48ms, CPU 점유율 **58~77% → 29~39%**.
- 튜너 갱신율 41.7Hz → 20.8Hz (튜너 UI로 충분).
- ⚠ `tuner.c`는 sim과 공유하는 파일이다. **상수 1개만** 변경하고 알고리즘·다른 상수는
  건드리지 말 것. `_Static_assert` 3개가 그대로 통과하는지 확인.

## ①-D 워치독 안전망
- `audio_task` 루프에 블록 카운터를 두고 **64블록마다(≈341ms) `vTaskDelay(1)`** 호출.
- 정상 상황에서는 ①-B/C로 `i2s_channel_read`가 블로킹하므로 불필요하지만,
  이상 상황에서 폭주 재발 시 IDLE1이 반드시 실행되도록 하는 안전망이다.

## 건① 완료 판정
- 튜너 앱을 **5분 이상** 켜둬도 워치독 리셋 없음.
- 오버플로 로그가 나오더라도 **2초에 1줄 이하**.
- 정상 상태에서 튜너 음정 표시가 종전과 동일.
- 다른 앱(monitor/images/bounce) 거동 무변경.

---

# 건② TRS 래치 — 과도 스윕 오래치 수정

## 증상 (실기 로그)
```
I (281493) ladder ratio=0.1104 -> DEADZONE (latch=HOME)   ← LEFT를 눌렀는데 HOME이 래치
I (283493) ladder ratio=0.0948 -> LEFT     (latch=HOME)   ← LEFT로 디코드돼도 래치가 안 바뀜
```
UP 연타 시 의도치 않게 앱이 실행된 것도 이 버그다(과도 구간에서 OK가 래치됨).

## 원인
무입력(ratio≈1.0)에서 키를 누르면 전압이 **위에서 아래로 쓸고 내려간다.** 32회 평균이 과도
구간에 걸치면 중간값이 나와 목표보다 위쪽 키(HOME·OK) 창에 먼저 걸린다. 현재 정책이
**"더 높은 비율로만 전환"** 이라 아래쪽 목표 키로 내려가는 것을 막아 오래치가 고정된다.

## ②-A 래치 갱신에 연속 안정성 요구
- 후보 상태(`decoded` 또는 `idle`)가 **연속 3회 동일**할 때만 래치를 갱신.
  카운터와 직전 후보를 정적 변수로 유지. `#define INPUT_TRS_STABLE_COUNT 3`.
- **기존 전이 규칙 4가지는 유지**(안정성 조건 통과 후 적용):
  1. 래치 없음 + 유효 키 → 래치
  2. 래치 있음 + idle → 해제
  3. 래치 있음 + **더 높은 비율**의 다른 키 → 전환
  4. 래치 있음 + 더 낮은 비율 / 데드존 → 무시(현재 키 유지)

⚠ 안정성 카운터는 **래치 갱신에만** 적용한다. `input_button_update()`의 디바운스/홀드/
리피트 상태머신 자체는 수정하지 않는다(상수 조정은 건④에서 별도로 다룬다).

---

# 건③ 판정표 재보정 (하드웨어 변경 반영)

## 회로 변경 완료
접점 저항(택트 스위치+브레드보드, 0~100Ω)이 작은 키 저항에서 큰 오차를 유발해 UP이 전혀
인식되지 않았다. Rtop을 키워 접점 저항 비중을 낮춘 새 회로로 교체 완료:

```
Ring(+3V3) ─[Rtop 10k]─┬─→ Tip ──→ G4 (ADC1_CH3)
                        │
   UP    ─ 직결(0Ω) ─ Sleeve      DOWN ─ [470Ω] ─ Sleeve
   LEFT  ─ [1k]     ─ Sleeve      RIGHT─ [2k]   ─ Sleeve
   OK    ─ [4.7k]   ─ Sleeve      HOME ─ [10k]  ─ Sleeve
```
- UP↔DOWN 간격 **32mV → 112mV**. 모든 인접 간격 112mV 이상.
- 최대 전압 1.61V(ADC 선형 구간), 소비 전류 0.33mA.

## ③-A 판정표 교체 (잠정값)
`app_main.c`의 6개 CENTER/WINDOW 상수를 아래로 교체한다.

| 키 | CENTER | WINDOW | 참고 @3208mV |
|---|---|---|---|
| UP | 0.0051 | 0.0117 | 0 ~ 54 mV |
| DOWN | 0.0505 | 0.0096 | 131 ~ 193 mV |
| LEFT | 0.0970 | 0.0101 | 279 ~ 344 mV |
| RIGHT | 0.1736 | 0.0387 | 433 ~ 681 mV |
| OK | 0.3285 | 0.0400 | 926 ~ 1182 mV |
| HOME | 0.5113 | 0.0400 | 1512 ~ 1769 mV |
| IDLE | ratio ≥ 0.85 | — | ≥ 2727 mV |

- 접점 저항 0~100Ω + 이전 실측에서 확인된 ADC 상향 편의(최대 +4%)를 반영.
- 57개 조합 검증: 2키 조합 오인식 **0건**. 3키 이상 2개 조합(`LEFT+RIGHT+OK` 계열)이
  DOWN으로 읽히지만 실사용에 없는 입력이므로 허용.
- 각 상수 옆에 유래(저항값·예상 전압)를 주석으로 남길 것.

## ③-B 로깅 유지
`INPUT_TRS_LOG`는 **1로 유지**. 새 회로의 실측 재보정이 아직 남아 있다.
로그 포맷 현행 유지(raw / mV / ratio / 판정 / latch / idle_ref).

---

# 건④ 반응성 개선 (최소 범위)

현재 래더 키의 입력 지연은 **폴링 10ms + 안정성 30ms + 디바운스 30ms ≈ 최악 70ms**다.
컴퓨터 키보드(5~20ms) 대비 3~10배 느려 내비게이터로서 체감 불편이 예상된다.

⚠ 이 작업의 목적은 "래더를 최적화"가 아니라 **공정한 사용성 평가를 가능하게 하는 것**이다.
불필요한 지연을 걷어낸 상태에서 실사용 체감을 봐야 Smart 컨트롤러 착수 여부를
올바르게 판단할 수 있다. 아래 범위를 넘는 튜닝은 하지 말 것.

## ④-A 폴링 주기
- `INPUT_POLL_MS` 10 → **5**.
- ADC 32회 평균의 실행 시간이 5ms 예산 안에 드는지 확인하고, 초과하면 평균 횟수를
  16회로 낮춘 뒤 **로그로 노이즈가 악화되지 않았는지 보고**할 것.

## ④-B 버튼별 디바운스
- `input_button_t`에 `debounce_ms` 필드를 추가한다.
- **래더 6키: 10ms** — 건②의 안정성 카운터(3회 = 15ms)가 이미 필터 역할을 하므로
  30ms 디바운스는 중복이다.
- **FOOTSW: 30ms 유지** — 실제 기계 접점이므로 기존 값이 필요하다.
- `INPUT_DEBOUNCE_MS`는 기본값 상수로 남기고, 버튼별 값이 없으면 이를 쓰도록 한다.

목표: 래더 키 지연 **5 + 15 + 10 ≈ 30ms**.

⚠ `INPUT_HOLD_MS(500)` / `INPUT_REPEAT_DELAY_MS(400)` / `INPUT_REPEAT_RATE_MS(120)`는
**변경 금지**. 이들은 사용자 체감 타이밍이지 지연 요소가 아니다.

---

# 건⑤ 문서 동기화 (SSOT — 필수)

## ⑤-A `hardware/NETLIST_SPEC.md §7` 갱신
현재 §7은 **구 회로(Rtop 4.7k / UP=0·DOWN=150·LEFT=470·RIGHT=1k·OK=2k·HOME=10k)** 로
낡아 있다. 아래로 교체한다.

```
TRS_SIG        : U1.G4 (ADC1_CH3 겸 디지털 겸용), R_ser 220Ω ─ TRS_MAIN.Tip
TRS_MAIN.Ring  : +3V3  (R_prot 100Ω 경유)      ← 5V 금지
TRS_MAIN.Sleeve: GND

Ring(+3V3) ─ Rtop 10k ─ Tip
Tip ─ 각 키 ─ Sleeve(GND):
  UP=0Ω, DOWN=470Ω, LEFT=1kΩ, RIGHT=2kΩ, OK=4.7kΩ, HOME=10kΩ

GPIO_RESERVED  : U1.G5, U1.G6, U1.G7, U1.G15, U1.G16  (미사용, Phase 2 예비)
FOOTSW         : U1.G17, SW7.1 (SW7.2 → GND)
```

검토 규칙에 아래를 추가한다:
- **Ring은 +3V3 고정. 5V로 올리면 Basic 컨트롤러 삽입 시 ADC 절대최대 초과로 G4가 손상된다.**
- `Rtop 10k`는 Basic에서 분압 상단, Smart에서 오픈드레인 버스 풀업을 겸한다. 등급 전환 시
  제거하지 않는다.
- `R_ser 220Ω`(Tip 직렬)은 ADC 전압에 영향이 없으며 Smart 모드의 고장 전류를 제한한다.
- `R_prot 100Ω`(Ring 직렬)은 **TRS 잭에 TS 플러그를 꽂았을 때의 3V3 단락**을 33mA로
  제한한다. 래더 소비가 0.33mA뿐이라 전압 강하는 무시 가능하다.
- **G4는 ADC 전용이 아니다.** PCB에서 RC 필터를 100nF 이하로 제한하고, 넷 이름을
  `TRS_SIG`처럼 용도 중립적으로 유지한다(Smart 모드 통신 대비).

## ⑤-B `ASSEMBLY.md` 갱신
- TRS 컨트롤러 절의 저항값을 새 회로로 교체.
- Tip 직렬 220Ω, Ring 직렬 100Ω 추가를 조립 절차에 반영.
- **"TRS 잭에 TS 케이블을 꽂으면 Ring이 단락된다"** 는 주의를 명시.

## ⑤-C `PROJECT_MASTER.md` 확장 트랙 추가
`## 3. 확장 트랙 아키텍처`에 **S6. 스마트 컨트롤러(MCU 6키)** 항목을 신설하고,
상세는 신규 문서 `CONTROLLER_DESIGN.md`를 참조하도록 링크한다.
`## 0. 문서 지도(SSOT 맵)`에도 `CONTROLLER_DESIGN.md`를 등재한다.

## ⑤-D `PUNCHLIST.md`
- "I2S overflow 관찰" 항목을 건①로 해결 처리하거나 현황 갱신.
- 해결된 `.pretty` 미배치 항목이 아직 남아 있으면 정리.

> `CONTROLLER_DESIGN.md`는 태윤이 별도로 추가한다. Codex가 새로 작성하지 말 것.

---

## 불변조건 (전체)
- `ui_event_t` enum·`EV_*`·`renderer_t`·`gadget_app_t` 인터페이스 변경 0.
- `INPUT_HOLD_MS` / `INPUT_REPEAT_DELAY_MS` / `INPUT_REPEAT_RATE_MS` 변경 0.
- `tuner.c`는 **`HOP` 상수 1개만** 변경. 알고리즘·필터·다른 상수 무변경.
- `music_events.c`·`fft_map.c`·오디오 정규화(`raw/2^31`)·rms/level 계산식 무변경.
- UI/앱/렌더러 파일 수정 0.
- `INPUT_TRS_LADDER=0` 빌드도 컴파일 성공.
- sim 빌드 무회귀(sim은 `tuner.c`를 공유하므로 `HOP` 변경 후에도 빌드·동작 확인).
- `input_button_read_raw()`의 **캡슐화 구조를 유지**한다. 이 함수가 Smart 컨트롤러 전환
  지점이므로, raw level 획득 경로를 상위로 노출시키지 말 것.

## 완료 판정
1. 튜너 앱 5분 이상 연속 동작, 워치독 리셋 없음.
2. 6키 각각 정확 인식. **특히 UP이 안정적으로 인식**될 것.
3. UP 연타 시 다른 키 이벤트나 앱 전환이 발생하지 않을 것.
4. 임의 키를 눌렀을 때 로그의 `latch`가 **처음부터 해당 키**(HOME/OK 경유 없음).
5. `RIGHT+OK` 동시 입력 → 이벤트 없음(데드존).
6. 키를 누른 채 다른 키 추가 → 처음 키 유지.
7. 방향키 오토리피트, HOME 롱프레스(런처 복귀), FOOTSW 단/장 누름 정상.
8. 방향키 이동이 종전 대비 체감상 빠를 것.

## 산출물
- `app_main.c` diff (로그 제한 / DMA / yield / 래치 안정성 / 판정표 / 폴링·디바운스).
- `tuner.c` diff (`HOP` 1줄).
- 변경 후 `analyze()` CPU 점유율 및 래더 키 지연 재계산 결과.
- ④-A에서 ADC 평균 횟수를 낮췄다면 그 사실과 근거.
- 문서 diff: `hardware/NETLIST_SPEC.md`, `ASSEMBLY.md`, `PROJECT_MASTER.md`, `PUNCHLIST.md`.
