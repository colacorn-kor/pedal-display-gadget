# Codex 지시서 S1 — 플랫폼 추상화 + LVGL PC 시뮬레이터

> 목적: UI/앱 코드를 **수정 없이** Windows에서 SDL 창(480×320)으로 실행 →
> 런처·테마·앱 디자인을 부팅 없이 즉시 반복. 같은 C 파일이 ESP 빌드에도 그대로 들어감.
> 순서 A(추상화)→B(시뮬). **A 완료 시점에 ESP 빌드가 기존과 100% 동일 동작**해야 함.

## Part A — 플랫폼 추상화 (ESP 빌드 무변화 리팩터)
1. 신규 `platform.h` — UI측이 소비하는 유일한 하드웨어 창구:
```c
void     plat_init(void);
uint32_t plat_millis(void);
bool     plat_input_poll(ui_event_t *ev);            /* 큐에서 이벤트 pop */
void     plat_nvs_load(void *blob, size_t n, bool *found);
void     plat_nvs_save(const void *blob, size_t n);
void     plat_audio_viz_get(audio_viz_snapshot_t *o);
void     plat_music_get(music_snapshot_t *o);        /* music_events 병합 후 */
void     plat_lvgl_lock(void); void plat_lvgl_unlock(void);
```
2. `platform_esp.c` — 기존 구현으로 위임(입력큐=현 input_task, NVS=app_slots의 nvs 호출부 이관,
   lock=lvgl_port_lock/unlock). **screen_manager/apps/theme/launcher/renderer는 plat_* 만 호출**하도록
   include·호출부 치환(로직 무변경). display_bringup·오디오태스크는 ESP 전용으로 그대로.
3. UI 순수 파일 목록을 `sim/UI_SOURCES.cmake`에 명시(양쪽 빌드가 같은 목록 공유).
4. ESP 빌드 검증: -Werror 통과 + 기존 전 기능 무회귀(이게 A의 완료판정).

## Part B — `sim/` 데스크톱 하네스 (CMake+SDL2, Windows/MSYS 또는 MSVC)
1. `sim/CMakeLists.txt`: LVGL 9.5를 FetchContent로(**ESP와 동일 마이너버전**), SDL2 링크,
   UI_SOURCES + `platform_sim.c` + `sim_main.c` 빌드. lv_conf: RGB565·UNSCII8/16 활성.
2. `platform_sim.c` 스텁:
   - 입력: 키보드 매핑 — 화살표=EV_UP/DOWN/LEFT/RIGHT, Enter=OK, H=HOME(길게=HOLD),
     F=FOOTSW(길게=HOLD). 길게 판정은 실제 펌웨어와 동일 임계(ms) 재사용.
   - NVS: `sim_nvs.bin` 파일 read/write (재실행 시 영속 재현).
   - 오디오: ①기본 = 합성 데모(느린 사인 스펙트럼 + 스페이스바=온셋 주입, 마우스휠=피치
     E2..E5) ②옵션 = `--wav 파일` 재생 입력(간단 RMS/FFT로 bars·온셋 생성; 품질은 근사면 충분).
   - lock: no-op. millis: SDL_GetTicks.
3. `sim_main.c`: SDL 창 480×320, lv_display SDL 백엔드, `sm_init()` 호출 후 lv_timer 루프.
4. 실행 문서 `sim/README.md`: 빌드 3줄(cmake -B build; cmake --build; 실행), 키맵 표.

## 불변조건
- **ESP 실동작 무변경**(Part A 후 회귀 0). 오디오 Core1 코드 이동 금지(ESP 전용 유지).
- UI 파일에 `#ifdef ESP` 남발 금지 — 분기는 platform_* 구현 파일로만.
- LVGL 버전 양쪽 일치. -Werror(ESP측) 유지, sim측도 -Wall.

## 완료 판정
- ESP: 기존 전 기능 정상(-Werror). / PC: 창에서 런처·테마전환·앱진입·풋스위치(F)·홈롱(H)
  전부 동작, 스페이스바 온셋으로 Bounce 점프, `sim_nvs.bin`으로 재실행 후 상태 유지.
## 산출물
platform.h/esp/sim, sim/ 일식, UI_SOURCES 목록, 치환 diff 요약, 양쪽 빌드 절차, 키맵.
