# LAUNCHER_DESIGN.md — 런처 / 앱 플랫폼 설계 (③)

> GG 페달. 이 문서는 **런처(홈)와 앱 플랫폼**의 설계 SSOT.
> `ARCHITECTURE.md`(전체 인앱 구조)와 분리. 런처·앱 계약·영속성·확장점만 다룸.
> 상태: **구현 SSOT**. 2026-07-26 기준 3행 내비게이션과 설정 팝업 반영.

## 0. 설계 철학 (확정)
- **열린 플랫폼("탈옥")**: 제조사는 **앱을 꽂는 틀(런처+로더)만** 제공. 콘텐츠는 커뮤니티가
  만들고 공유하고 유저가 직접 얹음. 스토어·서버 없음.
- **저작권 분산**: 기본 탑재는 오리지널/무해한 것만. 3자 콘텐츠는 유저가 개인 기기에
  직접 로드(폰 배경화면 논리). *법적 성립 여부는 제품화 시 별도 확인 필요 — 설계 철학일 뿐.*
- **단계적 실현(확정)**:
  - **③ 지금**: 네이티브 앱 기준으로 런처 UI + 앱 계약 + 영속성 완성.
  - **Phase 2+**: 스크립트 로더(SD에서 재빌드 없이 앱 로드)로 "진짜 폰처럼" 완성.
  - 이유: 계약(가)을 먼저 굳히면, 나중 로더는 그 계약을 스크립트로 노출만 하면 됨 → 헛수고 없음.

---

## 1. 앱 모델 = "판 깔기"의 실체 (`gadget_app_t`)
①에서 도입된 vtable을 3자 앱의 표준 계약으로 확정한다.

```c
typedef struct {
    const char *id;         /* 고유 id (영속성 키). 예 "monitor","rhythm" */
    const char *name;       /* 런처 표시명 */
    audio_mode_t audio_mode;/* SPECTRUM / TUNER / METER / NONE — 앱 요구 모드 */
    const lv_img_dsc_t *icon; /* ③ 신규: 런처 타일 아이콘. NULL이면 기본 아이콘 */

    void (*on_enter)(int variant);  /* 화면 빌드 (LVGL lock 안) */
    void (*on_exit)(void);          /* 화면 파괴·자원 해제 */
    void (*on_render)(void);        /* 매 프레임 (오디오 스냅샷 소비) */
    bool (*on_event)(ui_event_t e); /* 5키(상하좌우+확인) 위임. 처리시 true */

    void (*on_appearance_changed)(void); /* Color 적용 뒤 현재 화면 재스타일 */
    int (*mode_count)(void);
    const char *(*mode_name)(int idx);
    int (*mode_index)(void);
    void (*mode_set)(int idx);

    /* [Phase 2 예약] 라우팅 — Phase 1 전부 0 */
    app_input_source_t input_sources;
    app_output_route_t output_routes;
    int variant_count;
    platform_capability_mask_t required_capabilities; /* 0 = 추가 요구 없음 */
} gadget_app_t;
```

**3자 앱 작성자가 채우는 것(계약):** id·name·audio_mode·**icon**·4개 생명주기 콜백,
Color 재스타일 콜백·Mode 열거 훅과 필요한 플랫폼 capability.
(아이콘은 앱이 리소스로 제공 — 확정. 없으면 기본 아이콘으로 표시.)
**앱이 접근 가능한 것(제공 API):**
- 오디오: `audio_viz_snapshot_get()` (스펙트럼 bars/peaks/level, seqlock 스냅샷)
- 튜너류: 음정 스냅샷 API (튜너 앱이 쓰는 것과 동일)
- 렌더: LVGL 오브젝트를 `on_enter`에서 `lv_screen_active()` 자식으로 생성
- 입력: `on_event`로 5키. **홈/풋스위치는 매니저 소유 — 앱에 안 옴**(§ARCHITECTURE 5)

**앱이 하면 안 되는 것:** 오디오 코어/코덱 직접 제어, 전역 화면 파괴, 홈/풋스위치 가로채기.

> 이 계약이 곧 "게임 만들어 달라"고 남에게 줄 수 있는 인터페이스. 렌더+입력+오디오 스냅샷만
> 알면 리듬게임이 성립. 계약을 좁고 명확하게 유지 = 3자 진입장벽 최소화.

---

## 2. 슬롯 시스템 (앱 인스턴스 상태)
앱 정의(`gadget_app_t`, 코드/불변)와 **사용자별 배치 상태**(슬롯)를 분리.

```c
typedef enum { CHAIN_LIVE, CHAIN_STASH } chain_t;

typedef struct {
    const gadget_app_t *app;  /* 등록된 앱 정의 참조 */
    chain_t chain;            /* 라이브(순환) / 보관함(비활성) */
    uint8_t order;            /* 체인 내 위치 */
    uint8_t variant;          /* 선택된 변형 */
    uint8_t color;            /* Default/Blue/Green/Yellow/Red */
    uint8_t mode;             /* 앱별 화면 형식 */
    uint8_t options;          /* 앱별 소형 설정 비트 */
} app_slot_t;

static app_slot_t s_slots[APP_COUNT];
```

- **앱 id 인덱싱(미해결 결정)**: 영속성 키 안정성을 위해 **id 문자열 기준**으로 슬롯을 키잉
  (등록 순서 아님). 앱 추가/제거해도 기존 설정이 안 깨지게. → §5 참조.
- 라이브 체인 = 풋스위치로 순환하는 대상. 보관함 = 순환에서 빠진 앱.

---

## 3. 런처 UI (§ARCHITECTURE 8 구체화)
### 레이아웃 (480×320, LVGL)
```
┌───────────────────────────────────────────────┐
│  GUI                                    [배터리/상태 여백] │
│  ── LIVE ───────────────────────────────────   │  ← 라이브 줄(풋스위치 순환)
│   [Monitor] [Tuner] [Gallery] [Rhythm] …        │
│  ── STASH ──────────────────────────────────   │  ← 보관함 줄
│   [MIDI Mon] [Level] …                          │
│                         [Reorder]  [Settings]   │  ← 메뉴 항목 줄
└───────────────────────────────────────────────┘
```
- **타일**: 앱당 1개. name 라벨 + (있으면) 아이콘. 픽셀 폰트(로고 컨셉과 통일).
- **행 내비게이션**: 상·하만 `LIVE ↔ STASH ↔ 메뉴 항목`을 순환한다. 앱이 없는 행도
  건너뛰지 않고 섹션 라벨로 포커스를 표시한다.
- **행 내부 내비게이션**: 좌·우만 현재 행의 앱 또는 `Reorder ↔ Settings`를 순환한다.
- **화면 밖 표시**: 현재 보이는 네 타일보다 왼쪽이나 오른쪽에 앱이 더 있으면 해당 행의
  화면 가장자리에 삼각 화살표를 표시한다. 선택 행은 완전 불투명, 다른 행은 약하게 표시한다.
- **커서**: 선택 타일은 확대 상태와 3px 테두리를 함께 유지하고 우하단에 대각 화살표를
  겹친다. 메뉴 항목은 강조색 글자와 좌상단 대각 화살표를 사용한다.
- **확인(OK)**: 앱 타일 = 실행 / `Reorder` = 순서변경 / `Settings` = 설정 팝업.
- **홈**: 뒤로(런처가 최상위면 무동작). **풋스위치**: 라이브(직전 앱)로 복귀.
- 오디오 재생이 필요한 앱인데 플랫폼에 `AUDIO_PLAYBACK_OUTPUT` 능력이 없음 → 타일을
  흐리게 하고 진입을 막는다. 같은 가용성 검사는 부팅 복원과 풋스위치 순환에도 적용한다.

### 진입/복귀 규칙
- 부팅 시: 마지막 활성 앱 or 런처(설정에 따름).
- 라이브 모드에서 홈 롱 = 항상 런처로(안전망, §ARCHITECTURE 5).

---

## 4. 순서변경 모드 (§ARCHITECTURE 8 상태기계)
```
런처에서 [Reorder] 선택 → 순서변경 모드
  커서가 집게 모양으로 바뀜
  앱 선택 + 확인 = 집어듦(pick up)   [타일 들림 표시]
    좌·우 = 같은 줄 내 위치 이동
    상·하 = LIVE ↔ STASH 줄 이동 (= 활성/비활성 토글, chain 변경)
    확인  = 내려놓음(drop) → 위치 확정
  [Reorder]로 돌아와 확인 또는 홈 = 순서변경 종료 → 런처 기본
```
- "활성화"가 별도 UI 없이 **줄 옮기기로 통합**. 위=라이브, 아래=보관함.
- drop 시 `s_slots[].chain/order` 갱신 → 영속성 저장(§5).

### 설정 팝업
- 런처 `Settings` → `Theme`, `About`.
- `Theme` 아래에는 `Mode`, `Color`가 있다. Mode는 `Dark/Light`, Color는
  `Blue/Green/Yellow/Red`이며 확인으로 적용해 NVS에 저장한다. 조합된 팔레트는
  런처뿐 아니라 모든 앱의 공통 팝업 배경·패널·글자·강조색에도 적용된다.
- 앱 화면 홈 → `Settings`, `Info`. 세로 메뉴는 상·하로만 이동하며 좌·우는 항목 이동에
  쓰지 않는다. 앱 메뉴의 Exit 항목은 없으며 홈 길게 누르기가 즉시 런처 복귀를 유지한다.
- 모든 앱의 `Settings` 첫 항목은 `Theme`이다. Theme 아래의 `Mode`, `Color`가 앱 화면
  형식과 콘텐츠 팔레트를 각각 선택한다. Color는
  `Default/Blue/Green/Yellow/Red`이며, `Default`는 현재 런처 Theme을 상속한다.
  고정 Color는 앱 콘텐츠의 색만 바꾸되 전역 Dark/Light는 따르고, 런처나 공통 팝업
  팔레트에는 영향을 주지 않는다.
- Sound Monitor의 Mode는 `Curve`, `12-Band`, `Circular`, `Reference`다. 나머지 현재
  앱은 확장 위치를 일관되게 유지하기 위해 한 개의
  명명된 Mode를 제공한다. Sound Monitor Settings에는 Theme 뒤에 `Weighting`을 두며,
  네 항목은 `Flat/A-weighted/Flat(Loudness)/A-weighted(Loudness)`다. Sound Monitor
  화면의 상·하는 Weighting을 이전·다음으로 바꾸며, Curve의 좌·우만 단순화 수준을
  조정한다.
- 현재 구형 dB Meter Settings는 `Theme/Input/Window` 순서다. Input은 `LINE 2.00x`와
  `INST 7.82x`, Window는 `LIVE/AVG 1s/AVG 3s`를 제공한다. 화면에서는 좌·우로 두
  선택기 사이를 이동하고 상·하로 값을 바꾼다. 자동 듀얼레인지 빌드에서는 수동 Input을
  제거해 `Theme/Window`만 표시한다.

---

## 5. 영속성 (§ARCHITECTURE 10)
```c
typedef struct {
    chain_t chain; uint8_t order; uint8_t variant;
    uint8_t appearance; uint8_t options;
} app_setting_t;

typedef struct {
    /* 앱 id 문자열로 키잉 (등록순서 아님 — 안정성) */
    struct { char id[16]; app_setting_t s; } apps[APP_COUNT];
    char quick_app_id[16];   /* 풋스위치 롱 대상, 기본 "tuner" */
    /* [확장] 전역 테마·부팅 동작 등 추가 가능 */
} platform_config_t;
```
- **③ Phase 1**: NVS(비휘발성 저장)에 저장/로드. 하드코딩 기본값 → 첫 부팅 시 기록.
  (SSOT 원문의 "하드코딩 → 추후 SD"에서, ③은 **NVS까지** 올리는 걸 권장 — 재빌드 없이
  순서/변형 저장됨. SD 매니페스트는 Phase 2.)
- **저장 트리거**: 순서변경 drop, 변형·앱 Color/Mode 변경, 퀵앱 변경 시.
- **id 없는 슬롯**(앱 제거됨) = 무시. **새 앱**(설정에 없음) = 기본값으로 보관함 추가.
- 현재 `platform_config`는 **version 6**다. 기존 `local_theme` 1바이트를
  `appearance`로 재사용해 하위 3비트에 Color, 상위 5비트에 Mode를 저장하므로 v2~v5와
  blob 크기가 같다. v4의 Monitor 6개 프리셋은 같은 Color/Mode 조합으로 변환하고,
  v5 앱 White는 Blue로, 전역 White는 Light+Blue로 변환한다. 제거된 앱 ID는 로드 때
  무시하고 다음 저장에서 정리한다. 현재 구형 dB Meter의
  `options` 바이트는 LINE/INST와 LIVE/AVG 1s/AVG 3s 선택을 그대로 저장한다.
  자동 듀얼레인지 전환 시 INPUT 비트는 마이그레이션용으로만 읽고 UI에서는 제거한다.

---

## 6. 확장점 — 나중 스크립트 로더(Phase 2+)를 위한 설계
지금 네이티브로 짜되, 나중에 로더를 얹을 때 **구조를 안 바꾸도록** 미리 비워둘 것:
- **앱 등록이 동적**이어야 함: `app_registry_register(const gadget_app_t*)`가 런타임에
  호출 가능하게(현재도 그런 구조). 스크립트 로더는 SD에서 읽은 앱을 이 함수로 등록.
- **앱 정의를 "데이터"로 취급**: 네이티브 앱도 스크립트 앱도 같은 `gadget_app_t`로 보이게.
  스크립트 앱은 콜백이 "인터프리터 진입점"을 가리키는 thunk가 됨.
- **id 기반 영속성**(§5): 로더로 추가된 앱도 id만 있으면 설정이 붙음.
- **APP_COUNT 고정 배열 → 동적 상한** 고려: ③은 고정 배열로 충분하나, 로더 도입 시
  상한을 넉넉히(예: 32) 잡아두면 재설계 회피.

> 결론: ③에서 "동적 등록 + id 키잉 + gadget_app_t 데이터화"만 지키면, Phase 2 로더는
> **런처/영속성 재작성 없이** 얹힌다.

---

## 7. 앱 카탈로그 (현재)
| id | 이름 | audio | 변형 | 상태 |
|---|---|---|---|---|
| monitor | Sound Monitor | SPECTRUM | — | 4개 분석 Mode 동작 |
| dbmeter | dB Meter | SPECTRUM | — | 동작 |
| tuner | Tuner | TUNER | 기본/고급 | 동작 |
| images | Gallery | SPECTRUM | — | SD 이미지 탐색 동작 |
| music | Music | NONE | — | PC WAV·내장 로비, GG 코덱 대기 |
| game | Game | SPECTRUM | — | DMG `.gb`·빈 타일 내장 GG Cat 이스터 에그 |
| oscilloscope | Oscilloscope | SPECTRUM | — | PCM 시간파형·trigger·timebase/scale·hold |
| metronome | Metronome | NONE | — | PC click, GG 시각 모드 |
| midimon | MIDI Monitor | NONE | — | 채널 필터·pause·clear·clock 집계 |

---

## 8. 결정사항 (확정)
1. **영속성 저장소**: ③에서 **NVS**까지 구현. 재빌드 없이 순서/변형/퀵앱 저장. (SD 매니페스트는 Phase 2)
2. **부팅 동작**: **마지막 앱 복귀.** 런처 상태에서 종료했으면 런처로 복귀. (마지막 상태를 NVS에 저장)
3. **런처 타일**: **아이콘 포함.** 앱이 아이콘 리소스 제공(`gadget_app_t.icon`). 미제공 시 기본 아이콘.
4. **APP_COUNT 상한**: **고정 배열 32.** (나중 스크립트 로더 감안한 여유)
5. **첫 게임**: 나중. 게임은 "다양한 앱이 가능하다"의 예시일 뿐 — **런처 안정화 우선.**

---

## 9. 이후 Codex 지시서 분해 (설계 확정 후)
- **③-A 슬롯+영속성**: `app_slot_t`/`platform_config_t` + **NVS** 저장·로드 + id 키잉 + **마지막 앱/상태 복귀**.
- **③-B 런처 UI**: 2줄 레이아웃·커서·**아이콘 타일**(앱 제공 리소스, 폴백 기본)·구석 항목(LVGL).
- **③-C 순서변경 모드**: 집어듦/이동/줄전환/드롭 상태기계 + 저장 연동.
- **③-D 변형**: `variant_names` + 튜너 기본/고급 화면 분기.
- (별도) 리듬게임 앱: 계약(§1)만으로 작성 — 3자 위임 가능성 검증 겸.
