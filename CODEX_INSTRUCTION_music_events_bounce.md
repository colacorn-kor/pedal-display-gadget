# Codex 지시서 — 음악 이벤트 버스(피치·온셋·BPM) + 인터랙티브 데모 앱 "Bounce"

> 목적: 앱들이 기타의 **음높이/타이밍**을 단순화된 신호로 소비할 수 있는 공용 기반.
> (리듬게임·크로매틱 튜터·인터랙티브 애니 전부 이 위에서 성립 — "판 깔기"의 오디오판.)
> 구현 순서 A→B. ESP-IDF v5.4.4, -Werror.

## Part A — `music_events.h/.c` (Core 1 생산 / UI 소비)
1. 스냅샷(소비자 API, seqlock 더블버퍼 — `audio_viz_snapshot_get`와 동일 패턴, lock-free):
```c
typedef struct {
    uint32_t onset_seq;      /* 온셋마다 +1 (소비자는 로컬 last와 비교) */
    float    onset_strength; /* 마지막 온셋 세기(엔벨로프 대비 비율, ~1.0+) */
    uint32_t onset_ms;       /* 마지막 온셋 시각 esp_timer/1000 */
    float    level;          /* 현재 RMS 0..1 */
    float    bpm;            /* 러프 추정, 0=미확정 */
    bool     pitch_valid;    /* TUNER 모드 && voiced && clarity>0.8 */
    float    f0; const char *note_name; int octave; float cents; float clarity;
} music_snapshot_t;
void music_snapshot_get(music_snapshot_t *out);
```
2. **온셋 검출(모든 모드에서 상시, 블록당 O(1))** — 기존 Core 1 오디오 태스크의 블록 처리
   지점에 훅(새 태스크 생성 금지, malloc 금지):
   - 블록 RMS → 빠른어택(α≈0.5)/느린릴리즈(α≈0.02) 이중 엔벨로프.
   - `rms > 1.8 × slow_env + 0.008(플로어)` && 직전 온셋 후 **80ms 불응기** 경과 → 온셋.
   - strength = rms/slow_env. 상수는 #define으로 상단 모음(튜닝 대상 주석).
3. **피치**: `audio_get_mode()==AUDIO_TUNER`일 때 `tuner_get()` 결과를 스냅샷에 복사
   (`pitch_valid = voiced && clarity>0.8`). SPECTRUM 모드는 `pitch_valid=false`. 튜너 코드 무변경.
4. **BPM**: 최근 8개 온셋 간격 중 [250,2000]ms 범위만 취해 **중앙값** → 60000/median.
   유효 간격 4개 미만이면 bpm=0. (러프 추정임을 헤더 주석에 명시.)
5. `music_events_init()`을 오디오 초기화 직후 호출. CMakeLists SRCS 등록.

## Part B — 데모 앱 "Bounce" (신규 `app_bounce.c`)
1. `gadget_app_t`: id="bounce", name="Bounce", `audio_mode=AUDIO_TUNER`(피치+온셋 모두 필요),
   icon=자체 A8 32×32 제공. `apps_init()`에 등록(기본값 정책상 LIVE 끝에 편입됨).
2. **스프라이트**: **오리지널 픽셀 크리터**(단순 블롭/고양이류, 2프레임: idle/squash)를
   A8 32×32 C 배열로 Codex가 직접 생성(저작권 캐릭터 금지 — Nyan Cat 등 사용 불가).
   테마 accent색 리컬러. 지면선은 grid색, 배경 bg색(`theme_get()` 소비).
3. **동작(on_render, 매 프레임)**:
   - 물리: y속도에 중력, 지면 반발 0. **새 온셋(onset_seq 변화) → 위로 임펄스**
     (vy = -k×min(strength,3)), 착지 시 squash 프레임 1틱.
   - **피치 → 가로 위치**: f0를 MIDI로 환산, E2(40)..E5(76)를 화면 마진~폭에 선형 매핑,
     현재 x에서 목표로 lerp(0.2). pitch_valid=false면 중앙으로 서서히 복귀.
   - level → 스프라이트 크기 펄스(1.0~1.15). 상단 상태띠에 note_name+octave, bpm 표시(UNSCII_8).
4. 5키 사용: UP/DOWN=중력 프리셋 3단(무겁게/보통/달), OK=트레일 점 토글(최근 위치 8점, accent2).
   LEFT/RIGHT=미사용(true 반환 안 함).

## 불변조건
- **Core 1 오디오 경로에서 LVGL/heap 호출 금지.** 온셋 훅은 산술만. VIZ/튜너 파이프라인 무변경.
- seqlock 패턴은 기존 구현과 동일 방식(atomic index publish). `renderer_t`/슬롯/런처 무변경.
- -Werror 규칙 동일.

## 완료 판정
- Bounce 진입 → 기타(또는 손톱 탭) 온셋마다 점프, 음 높낮이로 좌우 이동, 무음 시 중앙 복귀.
- 시리얼 1초 로그(임시): onset_seq/bpm — 8분음 스트로크에서 bpm이 ±10% 내 수렴.
- SPECTRUM 앱들(monitor) 동작·프레임레이트 무회귀. -Werror 통과, 워치독 없음.

## 산출물
신규 `music_events.h/.c`, `app_bounce.c`(+아이콘/스프라이트 배열), 오디오 태스크 훅 diff,
CMakeLists. 상수 튜닝표(온셋 임계/불응기/중력 k)와 불변조건 체크리스트.
