# LAUNCHER_DESIGN.md — 런처 / 앱 플랫폼 설계 (③)

> GUI/GG 페달. 이 문서는 **런처(홈)와 앱 플랫폼**의 설계 SSOT.
> `ARCHITECTURE.md`(전체 인앱 구조)와 분리. 런처·앱 계약·영속성·확장점만 다룸.
> 상태: **설계안(태윤 검토용)**. 확정 후 Codex 지시서로 분해.

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
    audio_mode_t audio_mode;/* SPECTRUM / TUNER / NONE — 매니저가 enter 시 설정 */

    uint8_t  variant_count;         /* 변형 수(기본/고급 등). 1이면 단일 */
    const char *const *variant_names; /* ③ 신규: 변형 이름 배열(len=variant_count) */

    void (*on_enter)(int variant);  /* 화면 빌드 (LVGL lock 안) */
    void (*on_exit)(void);          /* 화면 파괴·자원 해제 */
    void (*on_render)(void);        /* 매 프레임 (오디오 스냅샷 소비) */
    bool (*on_event)(ui_event_t e); /* 5키(상하좌우+확인) 위임. 처리시 true */

    /* [Phase 2 예약] 라우팅 — Phase 1 전부 0 */
    app_input_source_t input_sources;
    app_output_route_t output_routes;
    bool needs_codec;               /* true면 코덱 부재 시 런처에서 비활성 표시 */
} gadget_app_t;
```

**3자 앱 작성자가 채우는 것(계약):** id·name·audio_mode·variant·4개 콜백.
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
│   [Monitor] [Tuner] [Images] [Rhythm] …         │
│  ── STASH ──────────────────────────────────   │  ← 보관함 줄
│   [MIDI Mon] [Level] …                          │
│                              [Reorder]  [Set]   │  ← 구석 항목
└───────────────────────────────────────────────┘
```
- **타일**: 앱당 1개. name 라벨 + (있으면) 아이콘. 픽셀 폰트(로고 컨셉과 통일).
- **커서**: 상하좌우 이동. 가리킨 타일 하이라이트(테마 accent).
- **확인(OK)**: 앱 타일 = 실행(→ 라이브 모드 진입) / 구석 항목 = 그 모드 진입.
- **홈**: 뒤로(런처가 최상위면 무동작). **풋스위치**: 라이브(직전 앱)로 복귀.
- `needs_codec=true`인데 코덱 없음 → 타일 흐리게(비활성), 진입 시 안내.

### 진입/복귀 규칙
- 부팅 시: 마지막 활성 앱 or 런처(설정에 따름).
- 라이브 모드에서 홈 롱 = 항상 런처로(안전망, §ARCHITECTURE 5).

---

## 4. 순서변경 모드 (§ARCHITECTURE 8 상태기계)
```
런처에서 [Reorder] 선택 → 순서변경 모드
  앱 선택 + 확인 = 집어듦(pick up)   [타일 들림 표시]
    좌·우 = 같은 줄 내 위치 이동
    상·하 = LIVE ↔ STASH 줄 이동 (= 활성/비활성 토글, chain 변경)
    확인  = 내려놓음(drop) → 위치 확정
  홈 = 순서변경 종료 → 런처 기본
```
- "활성화"가 별도 UI 없이 **줄 옮기기로 통합**. 위=라이브, 아래=보관함.
- drop 시 `s_slots[].chain/order` 갱신 → 영속성 저장(§5).

---

## 5. 영속성 (§ARCHITECTURE 10)
```c
typedef struct {
    chain_t chain; uint8_t order; uint8_t variant;
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
- **저장 트리거**: 순서변경 drop, 변형 변경, 퀵앱 변경 시.
- **id 없는 슬롯**(앱 제거됨) = 무시. **새 앱**(설정에 없음) = 기본값으로 보관함 추가.

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

## 7. 앱 카탈로그 (초기)
| id | 이름 | audio | 변형 | 상태 |
|---|---|---|---|---|
| monitor | Sound Monitor | SPECTRUM | — | 동작(①②) |
| tuner | Tuner | TUNER | 기본/고급 | 동작(변형은 ③) |
| images | Images | NONE | — | 동작 |
| rhythm | (미정) Rhythm Game | SPECTRUM/TUNER | — | ③ 이후 프로토타입 목표 |
| setlist/metronome/midimon | — | NONE | — | MIDI 앱(Phase 2 하드웨어 후) |

---

## 8. 태윤이 결정할 것 (설계 확정 전)
1. **영속성 저장소**: ③에서 **NVS**까지 올릴까(권장), 아니면 하드코딩 유지하고 SD는 Phase 2?
2. **부팅 동작**: 부팅 시 런처 표시 vs 마지막 앱 복귀 — 기본값?
3. **런처 타일 디자인**: 아이콘 포함 vs 텍스트만(픽셀폰트). 아이콘이면 앱이 아이콘 리소스도 제공?
4. **APP_COUNT 상한**: ③ 고정 배열 크기(예: 16? 32?) — 나중 로더 감안.
5. **첫 게임**: 런처 완성 직후 리듬게임 프로토타입을 바로 붙일지, 런처만 먼저 안정화할지.

---

## 9. 이후 Codex 지시서 분해 (설계 확정 후)
- **③-A 슬롯+영속성**: `app_slot_t`/`platform_config_t` + NVS 저장·로드 + id 키잉.
- **③-B 런처 UI**: 2줄 레이아웃·커서·타일·구석 항목(LVGL).
- **③-C 순서변경 모드**: 집어듦/이동/줄전환/드롭 상태기계 + 저장 연동.
- **③-D 변형**: `variant_names` + 튜너 기본/고급 화면 분기.
- (별도) 리듬게임 앱: 계약(§1)만으로 작성 — 3자 위임 가능성 검증 겸.
