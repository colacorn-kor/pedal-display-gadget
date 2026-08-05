# UI_DESIGN.md — GG 디자인 시스템 + 테마 아키텍처

> 상태: 확정안(태윤 미검토 항목은 §7 결정 로그 참조 — 뒤집기 가능)
> 대상 화면: 480×320 가로. LVGL 9.5. 브랜드: **GG**, 픽셀 아이덴티티.

## 1. 타이포그래피 — 픽셀 폰트
- **LVGL 내장 UNSCII** 사용(외부 에셋 불필요): `lv_font_unscii_16`(제목/타일명), `lv_font_unscii_8`(보조).
- 본문 가독용은 기존 Montserrat 유지(튜너 큰 숫자 등). **브랜드 요소(런처·타이틀·라벨)만 UNSCII.**
- sdkconfig: `CONFIG_LV_FONT_UNSCII_8=y`, `CONFIG_LV_FONT_UNSCII_16=y` 추가.

## 2. 테마 토큰 (전역)
모든 UI 크롬은 아래 6토큰만 사용(하드코딩 색 금지):
```c
typedef struct {
    char      name[16];
    lv_color_t bg;        /* 화면 배경 */
    lv_color_t surface;   /* 타일/패널 */
    lv_color_t text;      /* 기본 글자 */
    lv_color_t accent;    /* 커서/강조/선택 */
    lv_color_t accent2;   /* 보조 강조(피크·상태) */
    lv_color_t grid;      /* 구분선/그리드 */
} ui_theme_t;
```
### 내장 테마 조합
- 전역 Theme은 `Mode × Color`의 2×4 조합이다.
- Mode: `Dark`, `Light`. 배경·표면·기본 글자·그리드의 명암을 정한다.
- Color: `Blue`, `Green`, `Yellow`, `Red`. accent와 accent2를 정한다.
- 기본값은 `Dark + Blue`다. 이전 `BLUE/WHITE/GREEN` 설정은 각각
  `Dark+Blue/Light+Blue/Dark+Green`으로 자동 이전한다.

| Mode | bg | surface | text | grid |
|---|---|---|---|---|
| **Dark** | #101418 | #1C232B | #E8ECF1 | #2A333D |
| **Light** | #F2F4F6 | #FFFFFF | #22262B | #D5DAE0 |

| Color | Dark accent | Light accent |
|---|---|---|
| **Blue** | #4FC3F7 | #2F6FED |
| **Green** | #50D890 | #16845B |
| **Yellow** | #F4C95D | #B57A00 |
| **Red** | #FF6B6B | #C73E4D |

- 전역 테마는 런처와 모든 공통 팝업의 크롬(배경/패널/글자/커서)을 함께 관장한다.
- 앱 Color는 별개다. `Default`는 현재 전역 테마를 상속하고 고정
  `Blue/Green/Yellow/Red`는 앱 콘텐츠의 accent만 바꾼다. 고정 앱 Color도 전역
  Dark/Light는 따르며 런처와 공통 팝업 팔레트에는 영향을 주지 않는다.
  렌더러·게임 형식은 Color와 독립된 앱 Mode가 결정한다.

## 3. 다운로드 테마 (Phase 2 로더)
- 현재는 내장 2×4 조합만 구현하고 NVS에 Mode/Color 인덱스를 저장한다.
- 과거 단일 팔레트 `.ggt` 초안은 Dark/Light 쌍을 표현하지 못하므로 구현 계약에서
  제외한다. Phase 2에서 SD 테마 로더를 만들 때 두 명암 팔레트와 accent 계열을 함께
  담는 새 버전을 정의한다.
- 열린 플랫폼 철학은 유지한다. 제조사는 포맷과 로더를 제공하고 커뮤니티 테마를 허용한다.

## 4. 런처 화면 (③-B 시각 사양)
```
┌────────────────────────────────────────────────┐
│ GUI                              [UNSCII_16]    │  y=8  타이틀(좌), accent
│ LIVE ────────────────────────── [UNSCII_8,grid] │  y=40 섹션 라벨
│ ┌────┐ ┌────┐ ┌────┐ ┌────┐  … 가로 스크롤     │  타일 88×88, 간격 12
│ │icon│ │icon│ │    │ │    │                    │  아이콘 32×32(A8, text색 리컬러)
│ │name│ │name│ │    │ │    │                    │  이름 UNSCII_8
│ └────┘ └────┘ └────┘ └────┘                    │
│ STASH ─────────────────────────                 │  y=196 섹션 라벨
│ ┌────┐ ┌────┐ …                                 │  동일 타일(불투명도 60%)
│              [REORDER]   [SETTINGS]             │  하단 메뉴 행(UNSCII_8)
└────────────────────────────────────────────────┘
```
- **타일**: surface 바탕, radius 6, 테두리 1px grid. 아이콘 없으면 앱 이름 첫 글자(UNSCII_16)로 폴백.
- **커서**: 선택 타일 테두리 3px accent + 1px outline + 살짝 확대. 타일 우하단에는
  위·왼쪽을 향한 화살표, 하단 메뉴 좌상단에는 아래·오른쪽을 향한 화살표를 얹는다.
- **축 규칙**: 좌우=현재 행 내부 이동, 상하=LIVE↔STASH↔하단 메뉴 행 이동. 빈 앱 행도
  포커스 가능한 행으로 유지한다.
- **비활성**(플랫폼 능력 부족 등): 불투명도 40% + 진입 시 안내 토스트.
- 줄당 표시 4타일, 넘치면 좌우 스크롤(커서 따라).

## 5. 순서변경 모드 (③-C 시각)
- 집어든 타일: accent 배경 + 위로 6px 들림 + 그림자. 나머지 타일 살짝 어둡게.
- 순서변경 중 커서는 ASCII 집게 형태로 바뀌고, 집은 상태는 닫힌 집게로 표시한다.
- 상하로 줄을 옮기면 대상 줄 라벨을 accent로 표시한다. drop 시 배치와 NVS를 갱신한다.

## 6. 앱 화면 공통 규약
- 상단 상태띠(높이 20): 좌=앱 이름(UNSCII_8, text), 우=상태점(accent2, 예: 튜너 voiced).
- 앱 콘텐츠는 y=24 아래. 저장된 앱 Color를 `theme_for_app_color()`로 해석하고,
  씬 전용 색은 앱 자유다.
- **미디어(Gallery)** = SD `GG/images`의 정지 이미지와 GIF를 좌우로 탐색한다.
  앱 id `images`는 NVS·씬 호환을 위해 유지한다.

## 7. 결정 로그 ("알아서" 판단들 — 뒤집기 가능)
1. 픽셀폰트=**내장 UNSCII** (커스텀 폰트 에셋 제작은 나중 선택지).
2. 아이콘 포맷=**32×32 A8**(알파만, 테마 text색으로 리컬러 → 테마 자동 적응, 앱당 ~1KB).
3. 전역 UI Theme과 앱 Color/Mode **분리** (전역=런처+모든 공통 팝업,
   Color=각 앱 콘텐츠 팔레트, Mode=각 앱 화면 형식).
4. 테마 선택 UI=런처 하단 **[SETTINGS] → Theme → Mode/Color**. 각 목록은 상·하로
   선택하고 확인으로 적용+NVS 저장한다.
5. 모든 앱 Settings는 `Color/Mode/Info`를 제공하며 앱별 선택 항목을 그 사이에 추가할 수
   있다. Sound Monitor Mode는
   `Curve/12-Band/Circular/Reference`, Bounce Mode는 `Classic Cat`이다.
   Curve에서 좌·우는 `DETAIL/BALANCED/SIMPLE` 단순화 수준을 조정하며 상·하는 소비하지
   않는다. Reference도 방향키를 소비하지 않는다. Sound Monitor의 `Weighting`은
   `Flat/A-weighted`이고 기본값 Flat은 원 dBFS 분석을 그대로 표시한다.
6. platform_config **version 6**는 기존 1바이트 외형 필드에 Color 3비트와 Mode 5비트를
   패킹한다. v2~v5 blob 크기를 보존하며 v5 앱 White는 Blue로, 전역 White는
   Light+Blue로 이전한다. 이전 Monitor 프리셋은 대응 조합으로, 제거된 Nyan 값은
   `Default + Classic Cat`으로 마이그레이션한다.
7. 현재 구형 dB Meter 상단은 `INPUT LINE/INST`와 `WINDOW LIVE/AVG 1s/AVG 3s` 두
   선택기로 구성한다. 목표 자동 듀얼레인지가 활성화되면 INPUT 선택기는 없애고,
   자동 범위 상태는 기본 화면이 아닌 진단 정보에만 표시한다.
