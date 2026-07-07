# Codex 지시서 ③-B/C — 테마 엔진 + 런처 UI + 순서변경

> 근거: `LAUNCHER_DESIGN.md`, `UI_DESIGN.md`(시각 사양·토큰·치수는 이 문서가 SSOT).
> 구현 순서 A→B→C. **각 파트 완료 시점마다 단독 빌드 가능**해야 함.
> ESP-IDF v5.4.4, LVGL 9.5, -Werror.

## Part A — 테마 엔진 (신규 `theme.h/.c`)
1. `ui_theme_t`(UI_DESIGN §2 그대로 6토큰+name) + 내장 3종(CHARCOAL/IVORY/CRT, §2 표의 HEX 정확히).
2. API: `theme_init(void)` `const ui_theme_t* theme_get(void)` `int theme_count(void)`
   `int theme_index(void)` `void theme_set_index(int idx)` (범위 클램프, 즉시 적용을 위해
   내부 관찰자 콜백 1개 등록 지원: `theme_on_change(void(*cb)(void))` — 런처가 사용).
3. **영속화**: `app_slots.c`의 `platform_config_t`에 `uint8_t theme_idx; uint8_t reserved[7];` 추가,
   **version 1→2 범프**(v1 blob은 기존 로직대로 기본값 리셋 — 수용됨). slots에
   `uint8_t app_slots_theme(void)` / `void app_slots_set_theme(uint8_t)` 추가(저장 트리거 포함).
   theme.c는 init 시 slots에서 읽고, set 시 slots에 위임.
4. sdkconfig.defaults에 `CONFIG_LV_FONT_UNSCII_8=y` `CONFIG_LV_FONT_UNSCII_16=y` 추가.
   실행 안내에 "로컬 `sdkconfig` 삭제 후 재빌드(defaults 재생성)" 명시.

## Part B — 런처 UI (screen_manager 최상위 상태)
1. `gadget_app_t`에 `const lv_img_dsc_t *icon;` 필드 추가(A8 32×32, NULL 허용).
   기존 3앱 정의는 `icon=NULL`로 초기화(지정 이니셜라이저라 추가만으로 안전).
2. screen_manager에 상태 추가: `MODE_LIVE / MODE_POPUP(기존 홈팝업) / MODE_LAUNCHER / MODE_REORDER`.
   - 진입: **EV_HOME_HOLD → LAUNCHER** (어디서든, 아키텍처 §5 안전망). HOME 숏 팝업은 기존 유지.
   - LAUNCHER에서: 방향키=커서, OK=타일 실행/구석 액션, **FOOTSW 또는 HOME=직전 라이브 앱 복귀**.
   - 앱 실행 시 `app_slots_set_last_view` 갱신(기존 경로 재사용).
3. 레이아웃/시각: **UI_DESIGN §4 수치 그대로**(타일 88×88·간격12·아이콘 A8 리컬러(text색)·
   이름 UNSCII_8·타이틀 "GUI" UNSCII_16 accent·섹션라벨·STASH 60% 불투명·비활성 40%).
   아이콘 NULL이면 이름 첫 글자 UNSCII_16 폴백. 줄당 4타일, 커서 따라 좌우 스크롤.
4. 하단 구석 2개: `[REORDER]`(Part C 진입), `[THEME]`(커서 올리고 좌/우=theme_set_index 순환,
   즉시 전체 재스타일 — theme_on_change로 런처/상태띠 색 갱신).
5. 모든 색은 `theme_get()` 토큰만 사용. 하드코딩 금지.

## Part C — 순서변경 모드
1. LAUNCHER의 [REORDER] → MODE_REORDER. 상태기계: 커서이동 → OK=집어듦 → 좌우=order 이동,
   상하=LIVE↔STASH 이동(chain 변경) → OK=drop(확정) → **`app_slots_save()`**. HOME=모드 종료.
2. 시각: UI_DESIGN §5(들림 6px·accent 배경·줄라벨 점멸). drop 시 슬롯 order 재정규화(0..n 연속).
3. 라이브가 0개가 되는 drop은 허용하되, 풋스위치 순환 폴백(③-A의 first-registered)이 동작함을 확인.

## 불변조건
- 오디오 코어/seqlock/`audio_viz_snapshot_get` 무변경. LVGL은 기존 lock 규약 내.
- `sm_*`/`ui_event_t` 공개 API는 **추가만**. MODE_LIVE의 기존 3앱 동작·풋스위치 순환(③-A) 무회귀.
- `app_slots.h` 기존 API 시그니처 유지(테마 2함수 추가만).
- -Werror: 한줄 다중 if 금지, 미사용 정리, strncpy 널종단.

## 완료 판정
- HOME 롱 → 런처(2줄·아이콘/폴백 타일·커서). OK로 앱 진입, FOOTSW로 복귀.
- [THEME] 좌우 순환 → 즉시 색 변경 + 재부팅 후 유지(NVS v2).
- [REORDER]로 monitor를 STASH로 옮기면 풋스위치 순환에서 제외 + 재부팅 후 유지.
- v1 blob 자동 리셋 로그 1회 확인. -Werror 통과, 워치독 없음.

## 산출물
수정/신규 파일 목록, A/B/C 각 처리 요약, 불변조건 체크리스트, 실행 순서(sdkconfig 삭제 포함).
