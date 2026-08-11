# 인앱 아키텍처 설계 — App Platform

> 이 문서는 **사람이 읽고 구조를 확정**하기 위한 설계 명세다. 실제 코드 변경은
> 이 문서를 근거로 작성될 Codex 지시서(단계별)에서 다룬다. 코드 블록은 *설계 의도*를
> 보여주는 예시이며 최종 구현 디테일은 지시서/Codex가 확정한다.
>
> **구현 진척(2026-08-05 기준):** ① 앱 레지스트리/기존 화면 마이그레이션, ② 9종 입력
> 이벤트와 홈·풋스위치 정책, ③-A 슬롯/NVS, ③-B/C 런처·순서변경·테마를 구현했다.
> 런처는 `LIVE`·`STASH`·`Reorder/Settings`의 3행 내비게이션이며, 앱 홈 메뉴와
> `Settings/Info`, `Settings → Theme → Mode/Color` 계층도 동작한다. 전역 UI 테마는 런처와 모든 공통 팝업의
> 팔레트를 함께 바꾸고, 각 앱의 Color와 Mode는 콘텐츠 팔레트와 화면 형식을 독립적으로
> 바꾼다.
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
4. **기존 구조 재사용** — `renderer_t` 패턴을 `app_t`로 승격. I2S와 DSP 상태는 계속
   **Core1만 소유**한다. 앱 플랫폼은 현재 `screen_manager`가 있던 자리(Core0/LVGL)에 앉는다.
5. **매니저/앱 경계** — 매니저는 **풋스위치(숏/롱) + 홈(숏=공통 팝업 / 롱=즉시 나가기)**
   만 가로채고, 나머지 5키(상·하·좌·우·확인)는 전부 활성 앱에 위임.
6. **PC 우선 기준 구현** — 앱·UI·설정·미디어·게임은 PC 시뮬레이터에서 먼저 완성하고,
   ESP에는 같은 공통 코드를 빌드한다. 하드웨어 차이는 플랫폼 백엔드로만 격리한다.

---

## 2. 시스템 컨텍스트 (기존 구조와의 경계)

```
Core 1: I2S + DSP(fft_map/tuner) + seqlock 발행
        ↓ audio_viz_snapshot_get() / audio_waveform_snapshot_get() / tuner_get()
Core 0 (이 문서): LVGL + 앱 플랫폼  ← screen_manager 자리
        - 모든 앱 코드는 lvgl_port_lock 안에서 실행 (현재와 동일)
        - 입력: input_task(샘플링·판정) → UI queue
                → display_task(큐 배출·앱 플랫폼 디스패치·렌더)
```

**중요:** I2S와 DSP 상태는 Core1만 변경하고, 앱은 발행된 일관된 복사본만 소비한다.
스펙트럼·레벨은 `audio_viz_snapshot_get()`, 튜너는 `tuner_get()`, Oscilloscope는 앱이
활성일 때만 발행되는 `audio_waveform_snapshot_get()`을 사용한다.

`input_task`는 물리 입력을 샘플링해 이벤트를 큐에 넣는 데까지만 책임진다. 앱 전환,
LVGL 작업, NVS 쓰기는 `display_task`가 큐를 꺼낸 뒤 수행한다. 따라서 화면 생성이나
플래시 쓰기가 느려져도 그 구간의 짧은 버튼·풋스위치 입력을 놓치지 않는다.

### PC 기준 구현

PC에서는 SDL/LVGL 화면, 키보드 입력, WASAPI loopback 또는 캡처 장치, WinMM MIDI,
로컬 `GG/`
폴더와 설정 파일이 ST7796, TRS 입력, PCM1808, MicroSD와 NVS를 대신한다. 앱 생명주기와
상태 기계는 양쪽에서 동일하다. 오디오 출력과 Game Boy 런타임은 이미 공통 계약으로
구현됐다. MIDI는 공통 parser/service/app과 PC WinMM·ESP unavailable 백엔드로 나뉜다.
제품 역할과 기능 이식 순서는 `PC_SIMULATOR_PRODUCT.md`를 따른다.

### 플래시 파티션

GG의 16MB flash는 저장소의 `partitions.csv`가 정의하는 커스텀 표를 사용한다. 기존 장치의
설정을 보존하기 위해 NVS는 `0x9000`/`0x6000`, PHY는 `0xf000`/`0x1000`, factory 앱은
`0x10000` 시작 주소를 유지한다. factory와 두 OTA 슬롯은 각각 4MB다. 현재 펌웨어는
factory에서 부팅하며, OTA 슬롯과 `otadata`는 S3 WiFi/OTA 구현 전까지 예약 영역이다.
파티션 변경 플래시는 NVS를 먼저 백업하고 전체 flash erase 없이 수행한다.

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
typedef void (*app_appearance_fn)(void);
typedef int (*app_mode_count_fn)(void);
typedef const char *(*app_mode_name_fn)(int idx);
typedef int (*app_mode_index_fn)(void);
typedef void (*app_mode_set_fn)(int idx);

typedef struct {
    const char *name;
    app_mode_count_fn item_count;
    app_mode_name_fn item_name;
    app_mode_index_fn item_index;
    app_mode_set_fn item_set;
} app_choice_setting_t;

struct gadget_app {
    const char  *id;            /* "tuner" — 안정적 식별자(설정 키) */
    const char  *name;          /* "Tuner" — 표시명 */
    audio_mode_t audio_mode;    /* 이 앱이 오디오코어에 요구: SPECTRUM/TUNER/NONE */
    const lv_img_dsc_t *icon;   /* 런처 아이콘. NULL이면 이름 첫 글자 */

    app_enter_fn  on_enter;     /* 화면 빌드 + 자원 획득(뮤트/오디오모드) */
    app_exit_fn   on_exit;      /* 정리 */
    app_render_fn on_render;    /* 매 프레임(LVGL lock 안) */
    app_event_fn  on_event;     /* 위임된 키 처리. 소비하면 true */

    app_appearance_fn on_appearance_changed; /* Color 적용 뒤 현재 화면 재스타일 */
    app_mode_count_fn mode_count;
    app_mode_name_fn  mode_name;
    app_mode_index_fn mode_index;
    app_mode_set_fn   mode_set;
    const app_choice_setting_t *choice_settings; /* Settings 추가 선택기 */
    int choice_setting_count;

    app_input_source_t input_sources;  /* [Phase 2 예약] 0 = 기본 입력 */
    app_output_route_t output_routes;  /* [Phase 2 예약] 0 = 없음 */
    int                variant_count;  /* 변형 개수 */
    platform_capability_mask_t required_capabilities; /* 0 = 추가 요구 없음 */
};
```

`choice_settings`는 앱이 Settings의 `Theme` 뒤에 단일 선택 페이지를 등록하는 공통
UI 계약이다. 매니저는 이름·항목·현재값·적용 콜백만 디스패치하고 값의 의미와 저장은 앱이
소유한다. Sound Monitor의 `Weighting`과 dB Meter의 `Input/Window`가 이 계약을 사용한다.

앱 가용성은 물리 부품 이름이 아니라 `required_capabilities`와 플랫폼의 능력 마스크를
비교해 정한다. 현재 능력 타입은 `DISPLAY`, `AUDIO_ANALYSIS_INPUT`,
`AUDIO_PLAYBACK_OUTPUT`, `MEDIA_STORAGE`, `MIDI_INPUT/OUTPUT`, `GAME_RUNTIME` 자리를
정의한다. PC는 SDL 기본 출력 장치가 실제로 열린 경우에만 `AUDIO_PLAYBACK_OUTPUT`을
보고하고, 현재 코덱 없는 ESP는 이 비트를 보고하지 않는다. 매니저는 같은 검사로 런처
비활성 표시·직접 진입·부팅 복원·풋스위치 라이브 순환을 모두 처리한다.

레지스트리도 렌더러와 동일 패턴으로 구현됨(`gadget_app.c`):
```c
void                 app_registry_register(const gadget_app_t *a);
int                  app_registry_count(void);
const gadget_app_t  *app_registry_at(int idx);
int                  app_registry_find(const char *id);   /* 없으면 -1 */
const char          *app_registry_name(int idx);
bool                 app_registry_is_available(const gadget_app_t *app);
void                 apps_init(void);   /* renderers_init() 이후 네이티브 앱 등록 */
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
| 홈 롱 | **즉시 나가기**(안전장치) | — | 순서변경 종료 → 런처 |
| 홈 숏 | **표준 팝업 열기**(Settings/Info) | 뒤로 (첫 화면이면 무동작) | 순서변경 종료 → 런처 |
| 상·하·좌·우·확인 | **전부 활성 앱이 자유 해석** | 상하=행, 좌우=행 내부 / 확인=실행·진입 | 집어듦·이동·줄전환·내려놓음 |

> 홈 키는 **항상 매니저 소유**다. 라이브/오버레이에서 홈 숏 = 표준 팝업(공통 항목
> Settings·Info, 첫 선택 Settings), 홈 롱 = 즉시 나가기. 런처에서 홈 숏 = 뒤로
> (첫 화면이면 무동작). 팝업/앱 상태와 무관하게 홈 롱은 항상 런처로 빠져나온다(안전망).

### 매니저가 가로채는 것 vs 위임하는 것

- **매니저 전담**: `EV_FOOTSW`, `EV_FOOTSW_HOLD`, `EV_HOME`(팝업 열기), `EV_HOME_HOLD`(즉시 나가기).
- **활성 앱 위임**(라이브): `EV_UP/DOWN/LEFT/RIGHT/OK` → `app->on_event()`. **홈 키는 앱에
  가지 않는다** — 나가기·공통 메뉴는 매니저 표준 팝업이 담당하므로 앱은 메인 화면만 신경 쓴다.
- **팝업 열림 중**: 상·하 = 세로 항목 이동, 좌·우 = 무동작,
  확인 = 실행, 홈 숏 = 한 단계 뒤로, 홈 롱 = 즉시 나가기,
  풋스위치 숏/롱 = 팝업 닫고 앱 순환/튜너 점프.
- 런처/순서변경 모드에서는 활성 앱이 없으므로 5키를 매니저(런처 UI)가 사용.

→ 결과: 매니저는 **"앱 넘기기(풋스위치)" + "홈 키(팝업·나가기)"**. 5키(상하좌우+확인)는
전부 앱. 핑퐁이든 고급 튜너든 자유롭게 쓰고, 앱이 먹통이어도 홈 롱으로 항상 빠져나온다.

### 표준 팝업 메뉴 (홈 숏)

매니저 소유 표준 컴포넌트. 어떤 앱에서도 홈 숏으로 열린다. 첫 페이지 항목은
**Settings · Info**(첫 선택 Settings)다. 앱 메뉴에서 Exit를 제거해 설정 진입을 한 단계
줄이고, 나가기는 어느 앱에서나 홈 롱으로 유지한다.

- **Launcher Settings**: `Theme · About`. Theme 아래의 `Mode=Dark/Light`와
  `Color=Blue/Green/Yellow/Red`를 독립적으로 저장하고, 조합된 팔레트를 런처와 모든
  공통 팝업에 함께 적용한다.
- **App Settings**: 첫 항목 `Theme` 아래에 앱의 `Mode · Color`를 둔다. Info는 Settings
  밖의 앱 메뉴 두 번째 항목이며, 앱은 Theme 뒤에 자체 선택 설정을 등록할 수 있다.
  Color의 `Default`는
  현재 전역 UI 테마를 상속하고 `Blue/Green/Yellow/Red`는 앱 콘텐츠의 강조색만
  고정한다. 고정색도 전역 `Dark/Light`를 따르며, 어느 앱 선택도 런처나 공통 팝업
  팔레트에는 영향을 주지 않는다.
- **Sound Monitor**의 Mode는 `Curve/12-Band/Circular/Reference`다. Sound Monitor
  Settings는 `Theme/Weighting`이며 Weighting은
  `Flat/A-weighted/Flat(Loudness)/A-weighted(Loudness)`를 제공한다.
  색, 화면 형식과 weighting을 서로 독립적으로 저장한다.

### Sound Monitor 스펙트럼 표시 계약

- 분석 범위는 20Hz~20kHz 로그 주파수 축, 세로축은 `-72..0dBFS`다.
- Curve와 Reference의 그래프 배경은
  `Sub Bass 20~60 / Bass 60~250 / Low-Mid 250~500 / Mid 500~2k /
  High-Mid 2k~8k / High 8k~20kHz`를 여섯 개의 옅은 색 구간으로 표시한다.
  모든 구간명을 표시하되 `Sub Bass/Low Mid/High Mid`는 두 줄로 표시한다.
- 공통 분석 데이터는 모든 모드에서 **Slope 0인 무가중 dBFS**다. 청감상 평평하게 보이게
  하는 `dB/oct` 시각 기울기는 정확한 주파수 비교를 왜곡하므로 제공하지 않는다.
  앱의 `Weighting` 기본값 `Flat`은 이 값을 그대로 표시한다. `A-weighted`는 표준
  A-weighting 응답을 더한다. `(Loudness)` 두 항목은 1kHz를 0dB로 정규화한 60-phon
  등청감 참조의 역감도 보정을 더해 동일한 물리 레벨에서 덜 민감한 저·초고역을 낮게
  표시한다. `A-weighted(Loudness)`는 두 보정을 합산한다. 네 선택 모두 렌더링 복사본에만
  적용되어 원 스냅샷, dB Meter와 다른 앱의 값은 바꾸지 않는다. 실제 청취 SPL과 출력
  장치가 교정되지 않았으므로 Loudness는 비교용이며 `dBA`, phon 또는 절대 loudness
  측정값이 아니다. 앱 화면의 상·하는 네 Weighting을 직접 이전·다음으로 바꾼다.
  표시 하한 부근의 양의 보정은 6dB 구간에서 부드럽게 줄여 FFT release의 수치 잔여값이
  2~5kHz 신호처럼 드러나지 않게 하며, 그 위 레벨에는 전체 보정값을 적용한다.
- `Curve`는 공간 단순화를 `DETAIL/BALANCED/SIMPLE` 중에서 좌·우로 조정한다. 이 평활은
  로그 축에서 이웃 점을 섞는 렌더링 옵션이며 FFT 레벨과 다른 앱의 데이터는 바꾸지 않는다.
- `Reference`는 별도 모드다. 공간 평활도 끄고 입력의 상대 주파수 분포를 보여 준다.
  시간 평균·attack/release·peak hold도 적용하지 않는다. 단, FFT 창 자체의 시간 폭과
  PCM1808/프론트엔드의 실제 주파수 응답은 남으므로 교정 전에는 실험실급 분석기를
  뜻하지 않는다.
- 2Hz 1차 DC blocker로 0Hz 성분과 바이어스 이동을 제거한다. 첫 표시점 20Hz의 감쇠는
  0.05dB 미만이다. 0dBFS는 full scale이지 무신호가 아니므로 DC와 무신호는 표시 하한으로
  내려간다.
- 48kHz/2048-point 분석의 23.4375Hz bin을 로그 축 저역에 반복 복제하지 않는다.
  7-tap triangular anti-alias FIR와 4:1 감산을 두 번 사용해 12kHz/2048-point
  5.859375Hz 분석과 3kHz/2048-point 1.46484375Hz 분석을 함께 만든다. Reference는
  90~140Hz에서 3kHz→12kHz, 380~520Hz에서 12kHz→48kHz power를 로그 주파수 기준
  smoothstep으로 교차 혼합한다. 세 FFT의 창 길이는 다르지만 과거 스펙트럼을 보관해
  **창 중심 시각을 동일하게 맞춘 뒤** 혼합하므로 이동 sweep의 경계 이중 피크를 만들지
  않는다. 일반 Monitor는 12kHz와 48kHz 창 중심만 같은 방식으로 맞춘다.
  표시 band가 FFT bin보다 좁으면 중심 주파수의 power를 보간하고, 넓을 때만 band 안의
  최대 power를 사용한다.
- Curve와 12-Band의 FFT 파워는 65ms 평균, 즉시 attack, 220ms release를 사용하고 별도
  peak envelope를 유지한다. Curve/Reference는 현재 선과 반투명 채움만 그리며,
  peak hold는 12-Band의 밴드별 마커로만 표시한다. 초저역 FFT는 Reference에서만 실행해
  일반 모드의 Core1 부하를 늘리지 않는다.
- 본체와 PC 시뮬레이터는 이 매핑·시간 평활·release·peak hold를 구현한 동일한
  `fft_map.c`를 직접 빌드한다. 입력 수집은 I2S와 WASAPI/SDL로, FFT 실행 백엔드는
  ESP-DSP와 PC용 portable C로 나뉘지만 분석 정책은 시뮬레이터에서 별도로 재구현하지 않는다.
- 주/보조 로그 주파수 그리드와 12dB 간격 눈금을 표시하며, 렌더러 갱신은 약 30fps로
  제한한다. 캔버스와 작업 배열은 PSRAM에 둬 디스플레이 DMA용 내부 RAM을 침범하지 않는다.
- 기존 저장 씬과 NVS 호환을 위해 렌더러 ID `curve`와 기존 Mode 인덱스
  `Curve=0/12-Band=1/Circular=2`는 유지하고 `Reference=3`을 뒤에 추가한다.
- 12밴드 렌더러는 기타·베이스 그래픽 EQ의 주요 지점을 참고한
  `50/100/200/400/600/800/1.2k/1.6k/3.2k/4.5k/6.4k/10kHz` 중심 주파수를 사용한다.
  각 밴드 경계는 인접 중심 주파수의 기하평균이며, 기존 씬 호환을 위해 ID `bars`를 유지한다.
  프레임 길이가 같을 때 밴드 경계와 색 단계는 다시 계산하지 않는다.
- 얼굴형 `reactive` 렌더러는 제거하고, 72개 방사형 막대를 좌우 대칭으로 배치한
  `circular` 렌더러로 교체한다. 중심 원 크기는 고정하며 막대만 스펙트럼에 반응한다.
  위쪽 정점은 20kHz, 아래쪽 정점은 20Hz이며 오른쪽 37개 주파수 점을 왼쪽에 픽셀 단위로
  복제해 색과 형상이 정확히 대칭이다. 240px PSRAM 캔버스를 최대 약 16fps로 갱신하고,
  직전·현재 막대 반경의 합집합만 무효화하며 입력값이 안정되면 재그리기를 멈춘다.

### dB Meter 표시 계약

- `audio_viz_snapshot_t`는 기존 256점 스펙트럼과 함께 256샘플 블록의 정규화 RMS,
  sample peak, 전체 샘플 누적 에너지·개수를 같은 seqlock 스냅샷으로 발행한다.
  Core1 소유권과 발행 방식은 바꾸지 않는다.
- RMS와 sample peak의 dBFS는 각각 `20 log10(value)`로 표시한다. 따라서 full-scale
  sine은 RMS `-3.01dBFS`, sample peak `0dBFS`다. sample peak는 오버샘플링 true-peak가
  아니므로 화면에도 `SAMPLE PEAK`로 명시한다.
- **현재 구형 프로토타입**은 PCM1808의 명목 `3.0Vpp` full scale을 사용해 ADC 핀 전압을
  `ADC Vrms = normalized RMS × 1.5V`로 환산한다. 입력잭 전압은 선택한 물리 게인과
  맞춰 `ADC Vrms / 2.00`(LINE) 또는 `ADC Vrms / 7.82`(INST)로 역산한다.
  dBV 기준은 `1Vrms`, dBu 기준은 `0.775Vrms`다.
- 물리 SPDT 상태는 MCU에 연결되지 않았으므로 앱의 `INPUT LINE/INST`를 실제 스위치와
  수동으로 맞춘다. 표시값은 저항 명목값 기준 **입력잭 추정치**이며,
  `audio_config.h`의 모드별 correction factor는 1kHz 기준 1점 교정 연결점이다.
  교정 전에는 정확한 계측값으로 취급하지 않는다.
- 화면에서는 좌·우가 `INPUT ↔ WINDOW` 포커스를 고르고, 상·하가 선택한 값을 이전·다음으로
  바꾼다. 같은 상태를 `Settings → Input/Window`에서도 변경하며 두 경로는 같은 setter와
  지연 NVS 저장을 사용한다. 현재 구형 입력 빌드의 Settings 순서는
  `Theme/Input/Window`다.
- `WINDOW LIVE`는 최신 256샘플 블록(48kHz에서 약 5.33ms)의 RMS를 사용한다.
  `AVG 1s/3s`는 Core1의 누적 에너지·샘플 수 차분을 50ms 버킷에 보관해 선택 구간의
  전력을 평균한 뒤 RMS로 환산한다. 화면 갱신은 가독성과 입력 응답을 위해 200ms를 유지한다.
  sample peak는 각 표시 구간의 최댓값을 1초간 hold한다.
- 현재 AC 커플링과 PCM1808 내장 HPF를 통과한 오디오만 측정하므로 DC 전압계가 아니다.
  따라서 화면의 V 값은 오디오 대역 AC RMS이며 DC 전압계가 아니다.

### Oscilloscope 표시 계약

- `audio_waveform_snapshot_t`는 정규화 PCM을 signed 16-bit 2,048샘플로 보관한다. Core1이
  기존 I2S 블록에서 별도 double-buffer seqlock으로 발행하며 앱은 복사본만 읽는다.
- 파형 발행은 Oscilloscope의 `on_enter`에서 켜고 `on_exit`에서 끈다. 비활성 앱 때문에
  매 블록 4KB 복사가 계속 발생하지 않는다.
- 시간축은 2/5/10/20ms, 세로축은 `+/-0.10/0.20/0.50/1.00 FS`다. 최신 rising zero
  crossing을 화면 25% 지점에 맞추며 신호가 너무 작거나 crossing이 없으면 free-run한다.
- OK는 Hold/Run을 전환한다. 좌우는 시간축, 상하는 세로 감도를 조절하고 같은 값은
  Settings의 `Timebase`와 `Scale`에서도 바꾼다.

### GG 목표 자동 듀얼레인지 계약

- 제품 목표 회로는 물리 LINE/INST 스위치를 제거하고 같은 입력을 두 고임피던스 가지로
  나눠 PCM1808 양 채널로 동시에 읽는다. `VINL=HOT 약 0.13x`,
  `VINR=SENSITIVE 약 3.98x`다. HOT은 op-amp 전에 주파수 보상 수동 분압을 거치므로
  9V op-amp 전원 레일보다 큰 신호도 받을 수 있고, 명목 sine 입력 한계는 약 8.1Vrms다.
- Core1이 두 채널을 같은 입력잭 단위로 환산한 뒤, clip margin·overlap·hysteresis로
  포화되지 않은 쪽을 선택한다. 선택 범위가 바뀌어도 FFT·튜너·미터 값이 불연속적으로
  뛰지 않아야 한다.
- 사용자 dBFS는 개별 ADC 채널 full scale이 아니라 하나의 **GG Input Full Scale**에
  고정한다. ADC dBFS와 현재 범위는 진단 정보로만 노출한다.
- 절대 레벨의 기준은 교정된 입력잭 Vrms다. 범위별 1kHz gain과 20Hz~20kHz sweep LUT로
  저항·op-amp·커플링·PCM1808 편차를 보정한다.
- Instrument/Consumer(-10dBV)/Pro(+4dBu)는 레벨 맞춤용 비교선이며 정상/비정상 판정이
  아니다. 이 구조와 하드와이어 Thru의 계약은 `GG_PRODUCT_SPEC.md`가 권위다.
- `AUDIO_DUAL_RANGE`의 기본값은 `0`이다. 따라서 현재 구형 브레드보드 빌드는 PCM1808
  VINL만 읽는 기존 동작을 유지하며, 목표 HOT/SENSITIVE 두 채널이 모두 연결되기 전에는 `1` 변형을
  플래시하지 않는다.
- `AUDIO_DUAL_RANGE=1`은 32-bit stereo I2S 프레임의 left=HOT, right=SENSITIVE를
  동시에 읽는다. `audio_autorange`가 두 채널을 고정 GG 입력 스케일로 환산하고,
  SENSITIVE ADC peak 0.82에서 HOT으로 즉시 전환한다. 0.45 아래가 500ms 지속될 때만
  SENSITIVE로 돌아가며, 클리핑 전환이 아니면 한 블록 crossfade를 적용한다.
- I2S와 범위 선택은 계속 Core1만 소유한다. `audio_viz_snapshot_t`는 선택된 입력
  소스·GG 스케일 사용 여부·선택 범위 clip 상태와 두 ADC의 block RMS/peak 진단값을
  기존 seqlock으로 함께 발행하며 UI는 교정·선택이 끝난 복사본만 읽는다. 진단값도 UI가
  PCM/I2S 상태를 직접 읽는 경로를 만들지 않는다.
- 듀얼레인지 dB Meter는 LINE/INST 수동 선택을 없애고 `AUTO SENSITIVE/HOT`을 표시한다.
  RMS·peak dBFS는 고정 GG Input Full Scale, Vrms·dBV·dBu는 입력잭 기준으로 계산한다.
  따라서 Settings에도 수동 `Input`을 노출하지 않고 `Theme/Window`만 둔다.
  Mode의 `Range Diagnostics`는 HOT=VINL와 SENSITIVE=VINR의 ADC RMS/peak, 각 범위의
  입력잭 환산 Vrms, raw S/H 비와 교정 후 mismatch를 동시에 표시한다. 외부 9V 단독
  측정에서 USB 로그 없이 L/R 순서와 1kHz 교정을 확인하는 작업 화면이다.
- 현재 gain/correction은 명목값이므로 목표 회로의 1kHz·sweep 교정 전에는 계측 확정값이
  아니다. 1kHz 교정값은 `AUDIO_DUAL_HOT_VOLTAGE_CORRECTION`과
  `AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION` 빌드 정의로 주입할 수 있다. sweep 보정은
  실측표가 생기기 전에는 0dB 기본값을 유지하며, 측정값 없이 역보정 LUT를 추정하지 않는다.

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
│   [Monitor] [Tuner] [Gallery] [Scope] ...       │
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
- **Phase 2**: SD JSON 매니페스트 로드/저장. 펌웨어 재빌드 없이 컴파일된 앱의
  순서/활성화/변형/테마를 바꾼다. 임의 네이티브 앱 추가는 허용하지 않으며, 새 앱 코드는
  장기 스크립트/바이트코드 런타임이 생긴 뒤에만 SD로 배포한다.

저장 대상: 체인 배정 + 순서 + 변형 + 퀵 앱 id + 마지막 화면 + 테마.

현재 NVS schema v6는 기존 8바이트 앱 설정 크기를 유지하면서 `appearance`를
Color 3비트와 Mode 5비트로 패킹한다. v5의 `Default/Blue/White/Green`은 각각
`Default/Blue/Blue/Green`으로 이전하고, 전역 `Blue/White/Green`은
`Dark+Blue/Light+Blue/Dark+Green`으로 이전한다. 이 변환은 첫 v6 부팅 때 한 번 저장된다.

마지막 앱 id는 앱 전환 즉시 쓰지 않고, 같은 앱이 1초간 유지된 뒤 저장한다. 빠른 풋스위치
순환마다 플래시를 쓰지 않으면서 정상 종료·재부팅 복원에는 충분한 안정화 시간이다. 런처
복귀는 마지막 앱을 비우는 명시적 상태 변경이므로 즉시 저장한다.

---

## 11. 오디오 모드 / 뮤트 일반화

### 재생 출력 transport와 믹서

- `audio_playback.*`는 48kHz 스테레오 PCM, 단일 앱 ID 소유권, 재생/일시정지/정지 상태와
  `Music`/`Effects` 두 버스의 gain·큐·최종 클리핑을 공통으로 구현한다.
- 디코더와 앱은 장치를 직접 열지 않고 PCM을 공통 큐에 쓴다. 플랫폼 출력 소비자만
  `audio_playback_render()`로 블록을 가져간다.
- PC의 `sim_audio_output.*`는 SDL queued output을 사용하며 transport 상태 세대가 바뀌면
  이미 SDL에 쌓인 블록도 비운다. 자동 smoke는 실제 소리를 내지 않는 가상 sink로 같은
  스테레오 믹서 경로를 검사한다.
- 현재 ESP는 `audio_playback_init(false)`로 명시적 unavailable 상태를 만들며 큐 메모리를
  할당하지 않는다. 미래 코덱 백엔드는 Core1에서 같은 render API를 소비한다.
- 이 재생 경로는 AUX/헤드폰용이다. 하드와이어 메인 기타 Thru는 capability와 믹서 양쪽에
  포함되지 않는다.
- `AUDIO_NONE`은 Music처럼 분석 입력이 필요 없는 앱의 모드다. Core1은 I2S 수신 주기를
  유지하되 tuner/FFT/meter 처리를 건너뛰므로 재생 앱이 분석 상태를 불필요하게 갱신하지 않는다.
- `wav_decoder.*`는 저장소의 `FILE*`를 순차 읽는 공통 디코더다. PCM 8/16/24/32-bit와
  float32, mono/stereo, 8~192kHz WAV를 48kHz stereo float PCM으로 변환하며 UI와 출력
  장치를 소유하지 않는다.
- `metronome_engine.*`는 48kHz sample clock으로 40~220 BPM, 2~5박자와
  quarter/eighth/triplet/sixteenth tick을 만들고 첫 박·박·세분 click을 구분한다.
- `audio_effects.*`는 점프·장애물 통과·충돌 PCM을 코드로 생성해 `Effects` 버스에 쓴다.
  Game의 내장 게임은 실행 중에만 재생 소유권을 잡고 종료 시 해제하며, PC loopback이 자기
  효과음을 새 오디오 onset으로 감지하지 않도록 출력 직후 짧은 억제 구간을 둔다.
- Metronome·Game은 재생 capability를 필수로 선언하지 않는다. 출력 가능한 PC에서는
  소리를 내고, 현재 ESP에서는 동일 UI와 동작을 유지한 무음 시각 모드가 된다.

### 미디어 저장소와 Gallery

- `storage_scan_ex()`는 PC 폴더와 FATFS SD를 같은 계약으로 읽고 OK/Unavailable/IO Error,
  표시 수, 전체 허용 파일 수, 긴 경로 제외 수와 64개 상한 초과 여부를 반환한다.
- 카탈로그는 대소문자를 무시한 자연 정렬을 사용하고 같은 키는 원 경로로 결정론적으로
  정렬한다. 하위 폴더는 재귀 탐색하지 않는다.
- `image_probe.*`는 파일을 LVGL에 넘기기 전에 BMP/PNG/JPEG/GIF/BIN의 서명, 핵심 헤더와
  끝 구조를 검사한다. 앱은 다시 LVGL decoder의 치수 판정을 통과한 파일만 표시한다.
- Gallery는 Scanning/Loading/Ready/Empty/Error 상태를 가진다. 좌우 이동은 최종 요청만
  지연 로드하고, OK 재검색은 경로 기준으로 선택을 복원한다. 파일 형식·치수·크기는 하단에
  표시하며 긴 이름은 원본 경로를 보존한 채 한 줄 말줄임한다.
- 이미지가 정상 표시된 뒤 5초 동안 입력이 없으면 상·하단 정보 배너를 숨기고, 다음 버튼
  입력에서 즉시 다시 표시한다. 빈 상태·로딩·오류 화면은 필수 정보를 계속 표시한다.
- 빈 폴더·저장소 부재는 어두운 `GG` 월페이퍼를, 손상 파일은 다음 항목으로 이동할 수 있는
  항목별 오류 화면을 사용한다. 이 상태 기계는 PC와 ESP 공통 앱 코드다.

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
| `monitor` | Sound Monitor | SPECTRUM | — | `renderer_t` 중첩. Mode=`Curve/12-Band/Circular/Reference` |
| `dbmeter` | dB Meter | SPECTRUM | — | LIVE/1s/3s RMS, 입력잭 Vrms·dBV·dBu·dBFS |
| `tuner` | Tuner | TUNER | 기본/고급 | enter=뮤트. 고급=432/드롭/오프셋 |
| `images` | Gallery | SPECTRUM | — | 자연 정렬·손상 검사·메타데이터, 없으면 어두운 `GG` 월페이퍼 |
| `music` | Music | NONE | — | WAV 탐색·재생, 없으면 코드 생성 8비트 로비 음악 |
| `game` | Game | SPECTRUM | — | 검증된 DMG `.gb` + 빈 타일 OK=내장 GG Cat |
| `oscilloscope` | Oscilloscope | SPECTRUM | — | Core1 시간파형, trigger, timebase/scale, hold |
| `metronome` | Metronome | NONE | — | 40~220 BPM, 2~5박, 4종 분할. PC click, 현재 GG 시각 모드 |
| `midimon` | MIDI Monitor | NONE | — | 채널 필터·pause·clear·clock 집계 |
| `settings` | Settings / About | NONE | — | |

> 현재 실제 등록된 앱 = `monitor`·`images`·`tuner`·`dbmeter`·`music`·`game`·
> `metronome`·`oscilloscope`·`midimon` 9개.
> `music`은 `AUDIO_PLAYBACK_OUTPUT` 능력이 있는 플랫폼에서만 런처와 라이브 체인에 나타난다.
> `game`은 저장소나 외부 ROM이 없어도 내장 GG Cat으로 동작한다. 빈 타일에는 이름이나
> `built-in` 표기를 노출하지 않으며 OK로 실행되는 이스터 에그다. 게임 화면에도 이름을
> 표시하지 않는다. 런타임은 Chrome Dino식 대기·점프·가속·선인장·충돌 흐름을 따르고,
> 버튼 또는 임계 레벨 이상의 오디오 입력 상승 에지로 점프한다. 외부 ROM은 공통
> Peanut-GB 어댑터의 probe를 통과한 DMG `.gb`만 표시하며 현재 오디오는 무음이다.
> `metronome`은 출력이 없어도 시각 모드로 동작한다. MIDI Monitor도 물리 MIDI가 없어도
> 대기 화면으로 실행되며, PC에서는 WinMM 장치를 사용할 수 있다.

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
| `SCR_MONITOR` + `select_monitor_renderer` | **`monitor` 앱.** `renderer_t` vtable 중첩 | **완료** — Color와 `Curve/12-Band/Circular/Reference` Mode 분리 |
| `SCR_IMAGES` + 이미지 순환 | **`images`/Gallery 앱.** SD 카탈로그와 `on_event` 전환 | **완료** — 상태 UI·자연 정렬·검증·선택 보존 재검색 |
| `SCR_TUNER` + 뮤트 특별취급 | **`tuner` 앱.** enter=뮤트, audio=TUNER, variant=2 | **①·②완료** (뮤트/모드 자기소유, 입력 미소비) |
| `SCR_HOME` 메뉴 | **런처**로 역할 변경(단순 메뉴 → 두 체인 + 메뉴 행 + 순서/설정) | **완료** — 빈 행 포함 3행 내비게이션 |
| enum `screen_t` + 거대 switch | **활성 앱 인덱스 + 디스패치 + `s_slots[]` 모드** | **완료** |
| `sm_on_event`의 화면별 분기 | 매니저는 풋스위치/홈만, 나머지는 활성 앱 `on_event` 디스패치 | **②완료** — 매니저 전담 `{FOOTSW, FOOTSW_HOLD, HOME, HOME_HOLD}` + 표준 팝업, 5키 앱 위임 |
| `ui_event_t {PREV,NEXT,SELECT,BACK,FOOTSW}` | `{UP,DOWN,LEFT,RIGHT,OK,HOME,HOME_HOLD,FOOTSW,FOOTSW_HOLD}` | **②완료** — `app.h` 9종 교체, 낡은 enum 잔재 0 |
| (②신규) `input_task` 입력단 | 6버튼 + 풋스위치 상태기계(디바운스·롱프레스·오토리피트) | **②완료** (GPIO 1/2/4/5/6/13/7) |
| (②신규) 표준 팝업 | 매니저 소유 공통 메뉴와 설정 계층 | **완료** — 전역 UI Theme과 앱 Color/Mode를 분리한 공통 페이지 |

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

### 공통 MIDI 서비스

- 플랫폼 백엔드는 받은 바이트를 기존 `midi_feed()`에 전달하고, parser가 완성한 메시지는
  `midi_service`와 기존 `midi_map`으로 함께 발행한다.
- 서비스는 최근 비클록 메시지 16개, 전체·clock 수, 최신 Program Change와 sequence를
  seqlock snapshot으로 제공한다. WinMM callback은 자체 큐에만 쓰고 UI·NVS를 호출하지 않는다.
- capture는 `NONE/MONITOR` 단일 소유 모드다. MIDI Monitor가 활성화된 동안에는 메시지를
  앱이 먼저 소비하며 기존 scene/CC/clock 매핑은 중복 실행하지 않는다.
  앱을 나가면 `NONE`으로 돌아가 기존 매핑이 다시 동작한다.
- PC는 WinMM 입출력과 장치 열거를 제공한다. 현재 GG 플랫폼은 unavailable stub이며,
  미래 UART/BLE-MIDI도 같은 parser 진입점과 송신 API에 연결한다.

---

## 14. Phase 경계 요약

| 항목 | Phase 1 (지금) | Phase 2 (코덱/PCB 이후) |
|------|----------------|--------------------------|
| 앱 플랫폼·레지스트리·런처 | ✅ 구현 | — |
| 6버튼+홈+확인, 풋스위치 숏/롱, 오토리피트 | ✅ 구현 | — |
| 두 체인·순서변경·퀵 앱·변형 | ✅ 구현 | — |
| 설정 영속성 | NVS(id 기반 슬롯/순서/테마/마지막 앱/퀵앱) | SD JSON 로더 |
| SD 콘텐츠 | Gallery + Music WAV/내장 로비 + Game DMG `.gb`/내장 GG Cat | GG Music·외부 게임 오디오 출력, 추가 코어·스크립트 앱 |
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
- [x] 표준 팝업 실내용(App Settings/Info, Launcher Theme/About, 앱별 선택 훅)
- [ ] Phase 2 앱은 `requires_codec`로 비활성(라우팅 필드 예약은 ①에서 완료) *(Phase 2)*
