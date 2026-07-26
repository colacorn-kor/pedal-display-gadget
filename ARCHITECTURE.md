# 인앱 아키텍처 설계 — App Platform

> 이 문서는 **사람이 읽고 구조를 확정**하기 위한 설계 명세다. 실제 코드 변경은
> 이 문서를 근거로 작성될 Codex 지시서(단계별)에서 다룬다. 코드 블록은 *설계 의도*를
> 보여주는 예시이며 최종 구현 디테일은 지시서/Codex가 확정한다.
>
> **구현 진척(2026-07-26 기준):** ① 앱 레지스트리/기존 화면 마이그레이션, ② 9종 입력
> 이벤트와 홈·풋스위치 정책, ③-A 슬롯/NVS, ③-B/C 런처·순서변경·테마를 구현했다.
> 런처는 `LIVE`·`STASH`·`Reorder/Settings`의 3행 내비게이션이며, 앱 홈 메뉴와
> `Settings → Theme/Info` 계층도 동작한다. 전역 UI 테마는 런처와 모든 공통 팝업의
> 팔레트를 함께 바꾸며, 앱 안의 Theme은 해당 앱의 로컬 테마만 바꾼다. Sound Monitor와
> Bounce가 이 로컬 테마 계약을 구현한다.
> 진척 상세는 §13 표 참조.

---

## 0. 한 문장 요약

가젯을 **"앱 플랫폼"**으로 일반화한다. ~10개의 선택형 앱이 있고, 사용자는 **자기가 쓸
것만 활성화**해서 풋스위치로 전환한다. 현재 `screen_manager`를 앱 레지스트리로 일반화하며,
기존 `renderer_t`(곡선/12밴드/원형)는 **"Sound Monitor" 앱 안에 그대로 중첩**된다.
Phase 1 하드웨어(디스플레이 + 튜너 + 비주얼라이저 + 기타 탭)만으로 Phase 1 앱들이 전부
동작하고, Phase 2 오디오(코덱/믹스)는 구조가 **수용하되 요구하지 않는다**.

---

## 1. 설계 원칙

1. **Phase 1 자족** — 지금 하드웨어로 Phase 1 앱 전부 동작. Phase 2 앱은 등록만 하고
   하드웨어(코덱) 부재 시 비활성.
2. **신성한 원칙의 타입화** — 기타 메인 출력은 무손상 패시브. 앱이 메인 출력으로
   라우팅하는 것을 **언어적으로 불가능**하게 만든다(출력 경로 enum에 메인 부재).
3. **인지부담 최소** — 스마트폰 모델, 기본/고급 변형, **외울 제스처 0개**.
4. **기존 구조 재사용** — `renderer_t` 패턴을 `app_t`로 승격. **오디오 코어(Core1)는
   무변경.** 앱 플랫폼은 현재 `screen_manager`가 있던 자리(Core0/LVGL)에 그대로 앉는다.
5. **매니저/앱 경계** — 매니저는 **풋스위치(숏/롱) + 홈(숏=공통 팝업 / 롱=즉시 나가기)**
   만 가로채고, 나머지 5키(상·하·좌·우·확인)는 전부 활성 앱에 위임.

---

## 2. 시스템 컨텍스트 (기존 구조와의 경계)

```
Core 1 (무변경): I2S + DSP(fft_map/tuner) + seqlock 발행
        ↓ audio_viz_snapshot_get() / tuner_get()  (일관된 복사본)
Core 0 (이 문서): LVGL + 앱 플랫폼  ← screen_manager 자리
        - 모든 앱 코드는 lvgl_port_lock 안에서 실행 (현재와 동일)
        - 입력: input_task(샘플링·판정) → UI queue
                → display_task(큐 배출·앱 플랫폼 디스패치·렌더)
```

**중요:** 크로스코어 오디오 발행 구조(`s_viz_seq`, `publish_result` 등)는 **건드리지
않는다.** 앱은 기존 `audio_viz_snapshot_get()` / `tuner_get()`으로만 오디오를 소비한다.
앱 플랫폼은 순수하게 Core0/LVGL 측 일반화다.

`input_task`는 물리 입력을 샘플링해 이벤트를 큐에 넣는 데까지만 책임진다. 앱 전환,
LVGL 작업, NVS 쓰기는 `display_task`가 큐를 꺼낸 뒤 수행한다. 따라서 화면 생성이나
플래시 쓰기가 느려져도 그 구간의 짧은 버튼·풋스위치 입력을 놓치지 않는다.

---

## 3. `gadget_app_t` 인터페이스 + 레지스트리  *(①단계 구현됨)*

`renderer_t` 관용구를 그대로 승격한 vtable + 레지스트리. **①단계에서 실제 구현된 형태는
아래와 같다**(`gadget_app.h`). 타입명이 `gadget_app_t`인 것은 기존 `app.h`와의 심볼 충돌을
피하기 위한 Codex의 판단이며, 이 문서 전반의 "app_t"는 이 타입을 가리킨다.

```c
/* gadget_app.h — ①단계 구현 */
typedef enum {
    APP_INPUT_BUTTONS    = 1u << 0,
    APP_INPUT_FOOTSWITCH = 1u << 1,
    APP_INPUT_MIDI       = 1u << 2,
} app_input_source_t;                  /* [Phase 2 예약] */

typedef enum {
    APP_OUTPUT_DISPLAY    = 1u << 0,
    APP_OUTPUT_AUX        = 1u << 1,
    APP_OUTPUT_HEADPHONES = 1u << 2,
} app_output_route_t;                  /* [Phase 2 예약] — 메인 출력 값 부재(§4) */

typedef struct gadget_app gadget_app_t;
typedef void (*app_enter_fn)(int variant);
typedef void (*app_exit_fn)(void);
typedef void (*app_render_fn)(void);
typedef bool (*app_event_fn)(ui_event_t event);
typedef int (*app_local_theme_count_fn)(void);
typedef const char *(*app_local_theme_name_fn)(int idx);
typedef int (*app_local_theme_index_fn)(void);
typedef void (*app_local_theme_set_fn)(int idx);

struct gadget_app {
    const char  *id;            /* "tuner" — 안정적 식별자(설정 키) */
    const char  *name;          /* "Tuner" — 표시명 */
    audio_mode_t audio_mode;    /* 이 앱이 오디오코어에 요구: SPECTRUM/TUNER */
    const lv_img_dsc_t *icon;   /* 런처 아이콘. NULL이면 이름 첫 글자 */

    app_enter_fn  on_enter;     /* 화면 빌드 + 자원 획득(뮤트/오디오모드) */
    app_exit_fn   on_exit;      /* 정리 */
    app_render_fn on_render;    /* 매 프레임(LVGL lock 안) */
    app_event_fn  on_event;     /* 위임된 키 처리. 소비하면 true */

    /* 선택 사항. 모두 NULL이면 앱 Settings에서 Theme 항목을 숨긴다. */
    app_local_theme_count_fn local_theme_count;
    app_local_theme_name_fn  local_theme_name;
    app_local_theme_index_fn local_theme_index;
    app_local_theme_set_fn   local_theme_set;

    app_input_source_t input_sources;  /* [Phase 2 예약] 0 = 기본 입력 */
    app_output_route_t output_routes;  /* [Phase 2 예약] 0 = 없음 */
    int                variant_count;  /* 변형 개수 */
    bool               needs_codec;    /* [Phase 2 예약] 코덱 부재 시 런처 비활성 */
};
```

레지스트리도 렌더러와 동일 패턴으로 구현됨(`gadget_app.c`):
```c
void                 app_registry_register(const gadget_app_t *a);
int                  app_registry_count(void);
const gadget_app_t  *app_registry_at(int idx);
int                  app_registry_find(const char *id);   /* 없으면 -1 */
const char          *app_registry_name(int idx);
void                 apps_init(void);   /* renderers_init() 이후 3앱 등록 */
```

**생명주기:** `on_enter(variant)` → (매 프레임 `on_render()`, 입력 시 `on_event()`) →
`on_exit()`. `renderer_select`가 destroy→create 하던 것과 동일하게, 앱 전환 시 이전 앱
`on_exit()` 후 새 앱 `on_enter()`.

> 로컬 테마 훅은 앱이 실제 표시 중인 선택값을 매니저에 되돌려 주는 양방향 계약이다.
> 매니저는 슬롯의 저장값을 앱 진입 전에 적용하고, 앱 Theme 팝업은 같은 훅으로 이름과
> 현재 선택값을 읽는다. 따라서 메뉴 표시와 실제 앱 화면이 어긋나지 않는다.

---

## 4. 신성한 원칙의 타입화 (라우팅 — Phase 2 예약)

**①단계에서 실제 구현된 출력 경로 enum**(§3 참조)에는 **메인 출력 값이 존재하지 않는다**:

```c
typedef enum {
    APP_OUTPUT_DISPLAY    = 1u << 0,   /* 화면 */
    APP_OUTPUT_AUX        = 1u << 1,   /* 코덱 AUX (Phase 2) */
    APP_OUTPUT_HEADPHONES = 1u << 2,   /* 코덱 HP앰프 → 모니터 (Phase 2) */
    /* 메인 출력 값 없음 — 기타 메인 출력은 패시브, 소프트웨어가 못 건드림 */
} app_output_route_t;
```

앱 구조체의 `output_routes` 필드가 이 enum 타입이므로, **어떤 앱도 메인 출력을 선택할
이름 자체가 없다.** 이것이 신성한 원칙의 타입화다 — 런타임 검사가 아니라 언어 차원에서
불가능.

> **Phase 2 라우팅 설계 메모:** 위 enum은 ①단계에서 화면(DISPLAY)과 오디오(AUX/
> HEADPHONES)를 한 타입에 섞어 두었다. Phase 2에서 디지털 믹서를 본격 설계할 때는
> 입력 소스(기타탭/AUX/USB/합성음)와 출력 경로(모니터/USB녹음)를 의미축에 맞게
> 재정리하게 된다. 그때의 의도된 소스/출력 모델은 아래와 같다(현재 미구현):
> ```c
> /* Phase 2 의도 — 아직 구현 안 됨 */
> typedef enum { SRC_GUITAR_TAP=1, SRC_AUX=2, SRC_USB_IN=4, SRC_SYNTH=8 } audio_src_t;
> typedef enum { OUT_MONITOR=1, OUT_USB_REC=2 } audio_out_t;  /* OUT_MAIN 부재 유지 */
> ```
> 핵심 불변(메인 출력의 타입 차원 부재)은 어느 쪽 형태로 가든 유지한다.

`input_sources` / `output_routes`는 Phase 1에서 모두 0(미사용). Phase 2에서 앱이 "어떤
소스를 읽고 어떤 출력으로 내보내는지" 선언하면, 디지털 믹서가 그 매트릭스대로 라우팅한다.

---

## 5. 입력 모델  *(②단계 구현됨)*

### 이벤트 enum
```c
typedef enum {
    EV_UP, EV_DOWN, EV_LEFT, EV_RIGHT, EV_OK,
    EV_HOME, EV_HOME_HOLD,   /* 홈 숏 = 매니저 표준 팝업 / 홈 롱 = 즉시 나가기(안전망) */
    EV_FOOTSW, EV_FOOTSW_HOLD /* 풋스위치 숏 / 롱 */
} ui_event_t;
```

- **방향키 오토리피트**: 상·하·좌·우를 **누르고 있으면** 입력단이 해당 `EV_*`를 일정
  간격으로 재발행(값 조정/빠른 스크롤). 별도 이벤트 아님. 확인은 리피트·롱 없음.
- **롱프레스 감지 = 풋스위치와 홈, 둘만.** 나머지 손버튼은 숏프레스(방향키는 오토리피트).

### 동작 표 (확정)

| 입력 | 라이브/체인 모드 | 런처 — 기본 | 순서 변경 모드 |
|------|------------------|-------------|----------------|
| 풋스위치 숏 | 현재 체인 다음 앱 / (퀵 앱 오버레이면) 직전 복귀 | 라이브로 복귀 | — |
| 풋스위치 롱 | 퀵 앱 오버레이 진입 (이미 오버레이면 무동작) | 퀵 앱 오버레이 진입(직교) | — |
| 홈 롱 | **즉시 나가기**(=Exit 자동확정, 안전장치) | — | 순서변경 종료 → 런처 |
| 홈 숏 | **표준 팝업 열기**(첫 항목 Exit, 확인=나가기) | 뒤로 (첫 화면이면 무동작) | 순서변경 종료 → 런처 |
| 상·하·좌·우·확인 | **전부 활성 앱이 자유 해석** | 상하=행, 좌우=행 내부 / 확인=실행·진입 | 집어듦·이동·줄전환·내려놓음 |

> 홈 키는 **항상 매니저 소유**다. 라이브/오버레이에서 홈 숏 = 표준 팝업(공통 항목
> Exit·Settings, 첫 선택 Exit), 홈 롱 = 즉시 나가기. 런처에서 홈 숏 = 뒤로
> (첫 화면이면 무동작). 팝업/앱 상태와 무관하게 홈 롱은 항상 런처로 빠져나온다(안전망).

### 매니저가 가로채는 것 vs 위임하는 것

- **매니저 전담**: `EV_FOOTSW`, `EV_FOOTSW_HOLD`, `EV_HOME`(팝업 열기), `EV_HOME_HOLD`(즉시 나가기).
- **활성 앱 위임**(라이브): `EV_UP/DOWN/LEFT/RIGHT/OK` → `app->on_event()`. **홈 키는 앱에
  가지 않는다** — 나가기·공통 메뉴는 매니저 표준 팝업이 담당하므로 앱은 메인 화면만 신경 쓴다.
- **팝업 열림 중**: 상·하 = 세로 항목 이동, 좌·우 = 값 선택 페이지에서만 값 변경,
  확인 = 실행, 홈 숏 = 한 단계 뒤로, 홈 롱 = 즉시 나가기,
  풋스위치 숏/롱 = 팝업 닫고 앱 순환/튜너 점프.
- 런처/순서변경 모드에서는 활성 앱이 없으므로 5키를 매니저(런처 UI)가 사용.

→ 결과: 매니저는 **"앱 넘기기(풋스위치)" + "홈 키(팝업·나가기)"**. 5키(상하좌우+확인)는
전부 앱. 핑퐁이든 고급 튜너든 자유롭게 쓰고, 앱이 먹통이어도 홈 롱으로 항상 빠져나온다.

### 표준 팝업 메뉴 (홈 숏)

매니저 소유 표준 컴포넌트. 어떤 앱에서도 홈 숏으로 열린다. 첫 페이지 항목은
**Exit · Settings**(첫 선택 Exit)다. 여러 앱에 공통으로 필요하지만 메인 화면에 두면 혼잡한 것들의
집 → 앱 메인 화면을 깨끗하게 유지하고, 나가기 동작을 모든 앱에서 동일하게(홈 숏→확인, 또는
홈 롱) 만든다.

- **Launcher Settings**: `Theme · Info`. Theme은 전역 UI 테마
  `BLUE/WHITE/GREEN`을 좌우로 즉시 바꾸고, 런처와 모든 공통 팝업에 함께 적용한다.
- **App Settings**: 로컬 테마 훅을 구현한 앱만 `Theme · Info`를 보인다. 여기의 Theme은
  활성 앱만 바꾸며 전역 UI 테마에는 손대지 않는다. 훅이 없는 앱은 `Info`만 보인다.
- **Sound Monitor**는 6개 렌더러·팔레트 프리셋, **Bounce**는
  `Classic Cat/Nyan Cat`을 로컬 Theme 목록으로 제공한다.

### Sound Monitor 스펙트럼 표시 계약

- 분석 범위는 20Hz~20kHz 로그 주파수 축, 세로축은 `-72..0dBFS`다.
- 모니터 프리셋은 1kHz를 기준으로 `+4.5dB/oct` 시각 기울기를 적용한다. 이는 오디오
  신호를 바꾸는 EQ가 아니라 사람이 읽기 좋은 스펙트럼 모양을 위한 표시 보정이다.
- FFT 파워는 65ms 평균, 즉시 attack, 220ms release를 사용하고 별도 peak envelope를
  유지한다. Spectrum 렌더러는 5탭 공간 평활 뒤 현재 선·반투명 채움·피크선을 겹쳐 그린다.
- 주/보조 로그 주파수 그리드와 12dB 간격 눈금을 표시하며, 렌더러 갱신은 약 30fps로
  제한한다. 캔버스와 작업 배열은 PSRAM에 둬 디스플레이 DMA용 내부 RAM을 침범하지 않는다.
- 기존 저장 씬과 NVS 호환을 위해 렌더러 ID `curve`는 유지하고 표시명만
  `Spectrum (Blue/Green)`으로 바꾼다.
- 12밴드 렌더러는 기타·베이스 그래픽 EQ의 주요 지점을 참고한
  `50/100/200/400/600/800/1.2k/1.6k/3.2k/4.5k/6.4k/10kHz` 중심 주파수를 사용한다.
  각 밴드 경계는 인접 중심 주파수의 기하평균이며, 기존 씬 호환을 위해 ID `bars`를 유지한다.
- 얼굴형 `reactive` 렌더러는 제거하고, 72개 방사형 막대를 좌우 대칭으로 배치한
  `circular` 렌더러로 교체한다. 중심 원 크기는 고정하며 막대만 스펙트럼에 반응한다.
  240px PSRAM 캔버스를 최대 약 16fps로 갱신하고, 입력값이 안정되면 재그리기를 멈춘다.

### dB Meter 표시 계약

- `audio_viz_snapshot_t`는 기존 256점 스펙트럼과 함께 256샘플 블록의 정규화 RMS와
  sample peak를 같은 seqlock 스냅샷으로 발행한다. Core1 소유권과 발행 방식은 바꾸지 않는다.
- RMS와 sample peak의 dBFS는 각각 `20 log10(value)`로 표시한다. 따라서 full-scale
  sine은 RMS `-3.01dBFS`, sample peak `0dBFS`다. sample peak는 오버샘플링 true-peak가
  아니므로 화면에도 `SAMPLE PEAK`로 명시한다.
- PCM1808의 명목 `3.0Vpp` full scale을 사용해 ADC 핀 전압을
  `Vrms = normalized RMS × 1.5V`로 환산한다. dBV 기준은 `1Vrms`, dBu 기준은
  `0.775Vrms`다.
- 현재 아날로그 프론트엔드 이득은 교정되지 않았으므로 전압·dBV·dBu는 모두
  **ADC PIN NOMINAL**이다. 외부 잭 전압으로 환산하지 않으며, 실제 이득 교정은 별도 작업이다.
- 화면은 50ms마다 RMS 전력을 샘플링해 400ms 시정수로 평균하고 200ms마다 갱신한다.
  sample peak는 각 표시 구간의 최댓값을 1초간 hold한다.

### 하드웨어 영향

- 손버튼 6개 + 풋스위치 1개 = **입력 7핀**. **②에서 확정된 핀맵**: `UP=4, DOWN=5, LEFT=6,
  RIGHT=7, OK=16, HOME=15, FOOTSW=17`.
- 택트 ITS-1105 ×10 중 6개 사용(여유). **추가 구매 없음.**
- 추가 3핀(1·2·13)은 스트래핑(0·3·45·46)·USB(19·20)·I2S(15·16·17·18)·SPI/디스플레이
  (8·9·10·11·12·14)·뮤트(21)를 회피해 선정. 38·48은 DevKitC-1 온보드 RGB LED와 충돌
  가능해 제외. 입력단 파라미터: 디바운스 30ms / 롱프레스 500ms / 오토리피트 시작 400ms·간격 120ms.

---

## 6. 두 체인 (라이브 / 보관함) = 한 배열, 두 뷰

핵심 통찰: **"라이브 목록"과 "보관함"은 두 개의 자료구조가 아니라, 한 배열을 `chain`으로
필터링한 두 뷰**다.

```c
typedef enum { CHAIN_LIVE, CHAIN_STASH } chain_t;

typedef struct {
    const app_t *app;
    chain_t      chain;     /* LIVE = 풋스위치 순환에 포함 / STASH = 보관함 */
    uint8_t      order;     /* 자기 체인 내 위치 */
    uint8_t      variant;   /* 0=기본, 1=고급 ... */
} app_slot_t;

static app_slot_t s_slots[APP_COUNT];
```

- **순환** = "현재 보고 있는 앱이 속한 체인 안에서" 다음/이전. 라이브 앱이면 라이브 체인을,
  보관함 앱이면 보관함 체인을 돈다. **같은 함수, 인자만 현재 체인.**
- **활성화/비활성화** = `chain` 필드를 LIVE↔STASH로 바꾸는 것. 별도 토글 UI 없음.
- 보관함 앱에 진입해 있어도 풋스위치 = 그 체인의 다음 앱. 즉 두 체인은 **동작상 대칭**.

---

## 7. 퀵 앱 = 직교 오버레이

퀵 앱은 체인의 일부가 아니라 **체인 위에 잠깐 덮이는 별개 레이어**(즐겨찾기).

```c
static bool         s_quick_active;   /* 지금 퀵 앱 오버레이 중인가 → 뱃지 표시 */
static const char  *s_quick_app_id;   /* 롱프레스 대상. 기본 "tuner", 설정 가능 */
static launch_ctx_t s_saved_ctx;      /* 점프 직전 전체 상태(모드·체인·커서/앱) */
```

- **롱프레스**(라이브든 런처든, 어느 체인이든, 퀵 앱이 어디 속하든 무관) → `s_saved_ctx`
  저장, 퀵 앱 `enter()`, `s_quick_active = true`.
- 오버레이 중 화면에 **"퀵 앱" 뱃지** 표시(즐겨찾기로 잠깐 온 상태임을 알림).
- **숏프레스**(오버레이 중) → 퀵 앱 `exit()`, `s_saved_ctx` 복원, 뱃지 해제.
- 오버레이 중 **롱프레스 또 누름 → 무동작.**

**같은 앱, 다른 표시 상태:** 퀵 앱으로 띄운 튜너(뱃지 O)와 체인 안에서 진입한 튜너(뱃지 X)는
같은 앱이다. 퀵 앱은 별도 화면이 아니라 "이 앱을 오버레이 컨텍스트로 띄웠다"는 **플래그**다.

> **②까지의 상태:** ②는 풋스위치 롱 = 튜너로 **점프**까지만 구현(`footsw_quick_app`).
> `s_saved_ctx` 스냅샷·복귀·뱃지는 ③에서 이 오버레이 모델로 완성한다. 그래서 ②에서는
> 튜너에서 풋스위치 숏을 누르면 "직전 복귀"가 아니라 다음 앱으로 넘어간다(②에서는 정상).

**복귀의 모드 인식:** `s_saved_ctx`가 모드+체인+커서/앱을 통째로 스냅샷하므로, 라이브에서
띄웠으면 라이브의 그 앱으로, 런처에서 띄웠으면 **런처의 그 작업 화면**으로 정확히 돌아온다.

---

## 8. 런처 / 순서 변경 모드

### 런처 — 기본 레이아웃
```
┌───────────────────────────────────────────────┐
│  PEDAL DISPLAY                                  │
│  ── 라이브 ──────────────────────────────────   │  ← 위 줄: CHAIN_LIVE (순환 대상)
│   [Monitor] [Tuner] [Images] [Setlist] ...      │
│  ── 보관함 ──────────────────────────────────   │  ← 아래 줄: CHAIN_STASH
│   [MIDI Mon] [Level] ...                         │
│                              [순서변경]  [설정]  │  ← 구석 항목
└───────────────────────────────────────────────┘
```

- 상하좌우 → 커서 이동, 가리킨 앱/항목 하이라이트.
- 확인 → **앱이면 실행**(→ 라이브 모드) / **항목이면 그 모드 진입**(순서변경/설정).
- 홈 → 뒤로 (첫 화면이면 무동작). 풋스위치 → 라이브로 복귀.

### 순서 변경 모드
- 앱 선택 + **확인 = 집어듦**(pick up).
- **좌·우** = 같은 줄 내 위치 이동.
- **상·하** = **라이브 줄 ↔ 보관함 줄 이동** (= 활성/비활성 토글, `chain` 변경).
- **확인 = 내려놓음**(drop) → 위치 확정.
- 홈 = 순서변경 종료 → 런처 기본.

→ **활성화가 별도 UI가 아니라 "줄 옮기기"로 통합**. 위로 올리면 라이브, 아래로 내리면 보관함.

> **②까지의 상태:** 런처는 ①·②에서 **단순 1줄 메뉴**(레지스트리 순서대로 나열, 상하좌우로
> 커서, 확인=진입)까지만 구현. 2줄 체인·순서변경·구석 항목은 ③.

---

## 9. 앱별 변형 (기본 / 고급)

- `variant_count` + `variant_names`로 선언. slot의 `variant`에 사용자 선택 저장.
- `enter(variant)`가 변형에 맞는 화면을 빌드. 예: 튜너 기본 = 큰 음이름 하나, 고급 =
  A=432/드롭/오프셋 노출. **같은 코드베이스, 화면만 둘.**
- 런처/설정에서 앱별 변형을 고른다.

> ②까지: `variant_count`만 선언(튜너=2). `variant_names` 필드와 화면 분기는 ③.

---

## 10. 설정 영속성

```c
typedef struct {
    chain_t  chain;     /* LIVE / STASH */
    uint8_t  order;     /* 체인 내 위치 */
    uint8_t  variant;   /* 기본/고급 */
} app_setting_t;

typedef struct {
    app_setting_t apps[APP_COUNT];   /* 앱 id 인덱스로 키잉 */
    char          quick_app_id[16];  /* 롱프레스 대상, 기본 "tuner" */
} platform_config_t;
```

- **Phase 1**: 컴파일된 기본값(하드코딩). README가 예고한 "테이블 하드코딩 → 추후 SD
  manifest"의 그 자리.
- **Phase 2**: SD JSON 매니페스트 로드/저장(로더는 Phase 2 스텁). 펌웨어 재빌드 없이
  앱 추가/순서/활성화/변형/테마 변경.

저장 대상: 체인 배정 + 순서 + 변형 + 퀵 앱 id + 마지막 화면 + 테마.

마지막 앱 id는 앱 전환 즉시 쓰지 않고, 같은 앱이 1초간 유지된 뒤 저장한다. 빠른 풋스위치
순환마다 플래시를 쓰지 않으면서 정상 종료·재부팅 복원에는 충분한 안정화 시간이다. 런처
복귀는 마지막 앱을 비우는 명시적 상태 변경이므로 즉시 저장한다.

---

## 11. 오디오 모드 / 뮤트 일반화

- 각 앱이 `audio_mode`(SPECTRUM / TUNER / NONE) 선언.
- 매니저가 앱 `enter()` 시 `audio_set_mode()` 호출(현재 screen_manager가 하던 일을
  일반화). 앱 전환 시 자동으로 맞춰짐. *(②까지는 각 앱 `on_enter`가 직접 `audio_set_mode`를
  호출해 정상 동작 중이며, 매니저 일반화는 ③에서 정리.)*
- **뮤트의 일반화**: 튜너는 더 이상 특별 취급이 아니라 그냥 앱. 튜너 앱의 `enter()`가
  `mute_set(1)`, `exit()`가 `mute_set(0)` 호출. 풋스위치로 순환하다(또는 퀵 앱으로) 튜너에
  올라서면 뮤트, 떠나면 해제. 현재 동작을 더 깔끔하게 흡수.

---

## 12. 앱 카탈로그

### Phase 1 (지금 하드웨어로 동작)
| id | 이름 | audio_mode | 변형 | 비고 |
|----|------|-----------|------|------|
| `monitor` | Sound Monitor | SPECTRUM | — | `renderer_t` 중첩. 홈→Settings→Theme에서 6개 프리셋 선택 |
| `dbmeter` | dB Meter | SPECTRUM | — | RMS/피크 dBFS와 ADC 핀 기준 Vrms·dBV·dBu |
| `tuner` | Tuner | TUNER | 기본/고급 | enter=뮤트. 고급=432/드롭/오프셋 |
| `images` | Images | SPECTRUM | — | 이미지·폴더 탐색(좌/우 전환) |
| `setlist` | Setlist | NONE | — | MIDI PC → 곡·구간 텍스트(content_text 화면) |
| `metronome` | Visual Metronome | NONE | 기본/고급 | MIDI Clock BPM → 화면 플래시(소리는 Phase 2) |
| `midimon` | MIDI Monitor | NONE | — | 들어오는 MIDI 표시 |
| `settings` | Settings / About | NONE | — | |
| `bounce` | Bounce | SPECTRUM | — | 오디오 온셋 러너. 로컬 Theme=`Classic Cat/Nyan Cat` |

> 현재 실제 등록된 앱 = `monitor`·`images`·`tuner`·`bounce`·`dbmeter` 5개.
> 나머지는 카탈로그다.

### Phase 2 (코덱 의존 — 등록하되 `requires_codec=true`로 비활성)
| id | 이름 | 비고 |
|----|------|------|
| `drums` | Drum Machine | audio → 모니터 |
| `auxmon` | Music / AUX Monitor | audio → 모니터 |

코덱 부재 시 런처에서 회색/비활성 표시. 구조는 미리, 기능은 하드웨어 도착 후.

---

## 13. 기존 코드 마이그레이션 매핑 + 구현 진척

| 현재 (`screen_manager.c`) | 앱 모델 | 진척 |
|---------------------------|---------|------|
| `SCR_MONITOR` + `select_monitor_renderer` | **`monitor` 앱.** `renderer_t` vtable 중첩, 프리셋 API 제공 | **완료** — 앱 직접 상·하 변경을 제거하고 `Settings → Theme` 6개 프리셋으로 통합 |
| `SCR_IMAGES` + 이미지 순환 | **`images` 앱.** `on_event`로 이미지 전환 | **①·②완료** (②: `EV_LEFT/RIGHT` 순환) |
| `SCR_TUNER` + 뮤트 특별취급 | **`tuner` 앱.** enter=뮤트, audio=TUNER, variant=2 | **①·②완료** (뮤트/모드 자기소유, 입력 미소비) |
| `SCR_HOME` 메뉴 | **런처**로 역할 변경(단순 메뉴 → 두 체인 + 메뉴 행 + 순서/설정) | **완료** — 빈 행 포함 3행 내비게이션 |
| enum `screen_t` + 거대 switch | **활성 앱 인덱스 + 디스패치 + `s_slots[]` 모드** | **완료** |
| `sm_on_event`의 화면별 분기 | 매니저는 풋스위치/홈만, 나머지는 활성 앱 `on_event` 디스패치 | **②완료** — 매니저 전담 `{FOOTSW, FOOTSW_HOLD, HOME, HOME_HOLD}` + 표준 팝업, 5키 앱 위임 |
| `ui_event_t {PREV,NEXT,SELECT,BACK,FOOTSW}` | `{UP,DOWN,LEFT,RIGHT,OK,HOME,HOME_HOLD,FOOTSW,FOOTSW_HOLD}` | **②완료** — `app.h` 9종 교체, 낡은 enum 잔재 0 |
| (②신규) `input_task` 입력단 | 6버튼 + 풋스위치 상태기계(디바운스·롱프레스·오토리피트) | **②완료** (GPIO 1/2/4/5/6/13/7) |
| (②신규) 표준 팝업 | 매니저 소유 공통 메뉴와 설정 계층 | **완료** — 전역 UI Theme과 앱 로컬 Theme을 분리한 공통 페이지 |

**①에서 생성/변경된 파일:** `gadget_app.h`·`gadget_app.c`(인터페이스+레지스트리),
`app_monitor.c`·`app_images.c`·`app_tuner.c`(3앱), `screen_manager.c`(디스패처로 일반화),
`main/CMakeLists.txt`(빌드 등록). `sm_load_scene*`은 완전 재배선(앱 상태 경유).

**②에서 변경된 파일:** `app.h`(`ui_event_t` 9종 교체), `app_main.c`(`input_task` 입력
상태기계 + 버튼 GPIO 1·2·13), `screen_manager.c`(`sm_on_event` 상태기계 + 표준 팝업 +
①의 임시 풋스위치 브릿지 제거), `app_monitor.c`·`app_images.c`(`on_event` 새 어휘),
`midi_map.c`(CC 리맵). 풋스위치 숏=앱 순환·롱=튜너 점프, 홈 숏=팝업·롱=즉시 나가기.

**남은 일반화:** 변형 이름/화면 분기, Theme 이외의 앱별 설정 기여 훅, 코덱 의존 앱의 안내 UI.

**①·②에서 보존된 것:** Core1 오디오/seqlock, `renderer_t` 전체, LVGL lock 규약,
`content_screen`/`tuner_screen`/렌더러 구현(앱이 호출만 함), `sm_*` 공개 시그니처,
`gadget_app_t` 예약 필드, `midi.c` 파서.

---

## 14. Phase 경계 요약

| 항목 | Phase 1 (지금) | Phase 2 (코덱/PCB 이후) |
|------|----------------|--------------------------|
| 앱 플랫폼·레지스트리·런처 | ✅ 구현 | — |
| 6버튼+홈+확인, 풋스위치 숏/롱, 오토리피트 | ✅ 구현 | — |
| 두 체인·순서변경·퀵 앱·변형 | ✅ 구현 | — |
| 설정 영속성 | NVS(id 기반 슬롯/순서/테마/마지막 앱/퀵앱) | SD JSON 로더 |
| `in_sources`/`out_paths` 라우팅 | 필드 예약(0) | 활성(디지털 믹서) |
| 드럼/AUX 모니터 앱 | 비활성 스텁 | 동작 |

> 이 표는 **하드웨어 Phase 경계**(어느 HW에서 도는지) 기준이며, 소프트웨어 구현 진척과는
> 별개다. 진척은 §13 참조.

---

## 15. Codex에 위임할 결정 / 미해결

**②에서 확정됨:**
- 손버튼 GPIO 3개 = **1·2·13** (DevKit 핀맵·온보드 RGB LED 회피 반영).
- 입력단 파라미터 = **디바운스 30ms / 롱프레스 500ms / 오토리피트 시작 400ms·간격 120ms**.

**남은 것:**
- "퀵 앱" 뱃지의 시각 표현(화면 구석 작은 인디케이터).
- 앱별 팝업 기여 API와 변형 표시명 계약.

---

## 확정 체크리스트

- [x] 앱 전환 = 풋스위치 전용. 5키(상·하·좌·우·확인) = 앱. **홈 숏 = 매니저 표준 팝업,
      홈 롱 = 즉시 나가기** *(②)*
- [x] 매니저 가로채기 = 풋스위치(숏/롱) + **홈(숏=팝업 / 롱=나가기)** *(②)*
- [x] 6버튼(상하좌우+홈+확인), 방향키 오토리피트, 롱 감지 = 풋스위치·홈 둘만 *(②)*
- [x] 신성한 원칙 = 출력 enum에 OUT_MAIN 부재 *(①)*
- [x] 두 체인 = 한 배열 두 뷰, 활성화 = 줄 옮기기 *(③-A/C)*
- [x] 퀵 앱 = 직교 전환 + 상태 복귀 *(뱃지 제외)*
- [x] 표준 팝업 실내용(Settings/Theme/Info) *(앱별 기여 훅 제외)*
- [ ] Phase 2 앱은 `requires_codec`로 비활성(라우팅 필드 예약은 ①에서 완료) *(Phase 2)*
