# Codex 지시서 S1-b — PC 시뮬레이터 입력 리맵 + 실오디오 입력

> 전제: `CODEX_INSTRUCTION_pc_simulator.md`(S1)는 **이미 적용됨**. `sim/`이 빌드되어 `pedal_sim.exe`가
> 돈다. 이 지시서는 그 위에 두 가지를 얹는다: **(A) 가젯과 동일한 입력 매핑** + **(B) 실제 오디오 입력**.
> 원칙 유지: **UI/앱 소스(`screen_manager`·`app_*`·`renderer*`·`theme`·`launcher`) 무수정.**
> **ESP 빌드 무회귀**(이 지시서는 `sim/`과 sim 빌드 구성만 건드리고, 공유 DSP 소스는 *수정 없이 컴파일만* 추가).

---

## 배경 — 확정된 현재 상태 (근거)
- 현재 sim 키맵(`sim/platform_sim.c`의 `s_buttons[]`): 화살표=UP/DOWN/LEFT/RIGHT, `Enter`/`KP_Enter`=OK,
  **`H`=HOME**(길게 HOME_HOLD), **`F`=FOOTSW**(길게 FOOTSW_HOLD). `KEYDOWN` 특수처리에서 **`Space`=합성 온셋 주입**,
  `Esc`/`Q`=종료, 마우스 X=합성 피치.
- 현재 sim 오디오는 **전부 합성**이다. `plat_audio_viz_get`(사인 스윕), `synth_music_snapshot`(온셋/피치 합성),
  그리고 `tuner_init/reset/feed/get`·`music_snapshot_get`가 `platform_sim.c`에 **스텁**으로 들어 있다.
- 공유 DSP 이식성(확인함):
  - `tuner.c` → `math.h/stdatomic.h/stdint.h/string.h/audio_config.h/tuner.h`만 사용. **완전 이식 가능**(심 불필요).
  - `music_events.c` → `esp_timer.h`(=`sim/compat/esp_timer.h` 심 존재) + `app.h`/`tuner.h`. **이식 가능**.
    내부에서 `audio_get_mode()`(platform_sim 제공)와 `tuner_get()`(tuner.c 제공)을 호출해 **피치를 스냅샷에 병합**한다.
  - `fft_map.c` → `esp_dsp.h` 의존이라 **재사용 불가** → 스펙트럼 막대는 sim 자체 FFT로 만든다(아래 B-3).
- 가젯 오디오 태스크(`app_main.c` `audio_task`) 규격(sim이 그대로 미러링):
  - 블록 = **256 샘플**(`DMA_FRAMES`), `sample = raw/2^31`, `rms = sqrt(mean(sample^2))`, `level = clamp(rms*3, 0, 1)`.
  - `AUDIO_TUNER` 모드: `tuner_feed(samples,n)` + `music_events_process_block(rms,level)`.
  - `AUDIO_SPECTRUM` 모드: `music_events_process_block(rms,level)` + `fft_feed(...)`(→ sim에선 자체 FFT).
- sim이 컴파일하는 UI 소스 중 **`fft_map_*` 심볼을 참조하는 파일 없음** → `fft_map.c` 미포함으로 링크 문제 없음(확인함).

---

## Part A — 입력 매핑을 가젯과 일치 (`sim/platform_sim.c`)

가젯 물리 입력 ↔ PC 키 대응을 아래로 **변경**한다. 목적: 태윤이 가젯에서 쓰는 조작 감각을 PC에서 그대로 재현.

| 가젯 입력 | 이벤트 | PC 키 (변경 후) | 비고 |
|---|---|---|---|
| 상/하/좌/우 | EV_UP/DOWN/LEFT/RIGHT | **방향키 ↑↓←→** | 오토리피트 유지(그대로) |
| OK | EV_OK | **Enter**(+`KP_Enter`) | 그대로 |
| HOME | EV_HOME / EV_HOME_HOLD | **Backspace** (`SDLK_BACKSPACE`) | ← `H`에서 변경. 길게=HOLD |
| FOOTSW | EV_FOOTSW / EV_FOOTSW_HOLD | **Space** (`SDLK_SPACE`) | ← `F`에서 변경. 길게=HOLD |

구체 작업:
1. `s_buttons[]`에서 HOME 항목 `.key = SDLK_h → SDLK_BACKSPACE`, FOOTSW 항목 `.key = SDLK_f → SDLK_SPACE`.
   (hold 임계·이벤트 필드는 그대로 — 홀드 판정 로직 재사용.)
2. `sdl_event_watch`의 `KEYDOWN` 분기에서 **`if (key == SDLK_SPACE) { trigger_onset }` 특수처리를 제거**한다.
   이제 Space는 일반 버튼(FOOTSW)으로 `button_down/up`을 타야 한다. (Space가 quit/onset로 새지 않게 순서 주의.)
3. 합성 온셋 수동 주입은 **`O`(`SDLK_o`)로 이동**한다 — 단, **오디오 폴백(합성) 모드에서만** 의미가 있다(B-4).
   실오디오가 열리면 온셋은 실제 소리에서 나온다.
4. 종료 키는 **`Esc`만** 유지한다(`Q`는 제거 — 텍스트 입력 감각상 맨글자 종료는 사고 위험). 마우스 X=합성 피치는 폴백용으로 유지.
5. `sim/README.md`의 Controls 표를 위 매핑으로 갱신.

---

## Part B — 실제 오디오 입력 (SDL2 캡처: 스테레오 믹스 / 마이크)

### B-1. 공유 DSP를 sim 빌드에 편입 (수정 없이 컴파일만)
- `sim/UI_SOURCES.cmake`에 **`tuner.c`와 `music_events.c`를 추가**(또는 sim 전용 소스 리스트로 add_executable에 추가).
  두 파일은 **절대 수정하지 말 것**(ESP 빌드와 동일 소스 공유가 핵심).
- 그 결과 **`platform_sim.c`의 아래 스텁들은 반드시 삭제**한다 — 안 지우면 실제 심볼과 **중복 정의(링크 충돌)** 발생:
  - `tuner_init` / `tuner_reset` / `tuner_feed` / `tuner_get`  → `tuner.c`가 제공
  - `music_snapshot_get` / `synth_music_snapshot`             → `music_events.c`가 제공
- `platform_sim.c`에 **남겨야 하는 것**: `audio_set_mode/get_mode`, `audio_set_viz_mode`, `mute_set/get`,
  그리고 `plat_*` 진입점들. (`audio_get_mode()`는 `music_events.c`가 호출하므로 반드시 유지.)

### B-2. 캡처 장치 열기 (풀 모델 — 콜백 아님)
- SDL 오디오 캡처를 **풀 모델**로 쓴다: `SDL_OpenAudioDevice(name, /*iscapture=*/1, want, got, 0)` 후
  프레임 루프에서 `SDL_DequeueAudioSamples`로 뽑는다. (오디오 콜백 스레드에서 DSP/LVGL을 만지지 않기 위함 → 락 불필요.)
- 요청 포맷: `freq=48000`(=`AUDIO_SAMPLE_RATE`), `format=AUDIO_F32SYS`, **`channels=1`**, `samples=256`.
  스테레오만 잡히면 SDL이 모노로 다운믹스하도록 `allowed_changes=0`으로 강제(또는 2채널 받아 `(L+R)/2` 직접 다운믹스).
- 장치 선택:
  - 인자 없음 → SDL 기본 캡처 장치(`name=NULL`).
  - `--list-audio` → `SDL_GetNumAudioDevices(1)`+`SDL_GetAudioDeviceName(i,1)`로 인덱스·이름 출력 후 종료.
  - `--audio-device N` → N번 캡처 장치 사용.
  - 선택된 장치명을 시작 시 1줄 로그로 출력.
- **Windows 안내(README)**: 기타/라인 소리를 그대로 미러링하려면 사운드 설정에서 **"스테레오 믹스(Stereo Mix)"** 를
  활성화하면 캡처 목록에 뜬다. 없으면 마이크가 잡힌다. (스테레오 믹스는 재생 중인 오디오를 그대로 입력으로 돌려줌.)

### B-3. 블록 펌프 (app_main.c 규격 그대로 미러링)
프레임 루프(또는 `plat_input_poll`/전용 `plat_audio_pump` 내부)에서, 큐에 **256 샘플** 이상 쌓일 때마다 한 블록 처리:
1. `rms = sqrt(mean(sample^2))`, `level = clamp(rms*3, 0, 1)` — **app_main.c와 동일 식**(다르게 만들지 말 것).
2. `audio_get_mode() == AUDIO_TUNER`이면 `tuner_feed(block,256)` + `music_events_process_block(rms,level)`.
   아니면(`AUDIO_SPECTRUM`) `music_events_process_block(rms,level)` + **sim FFT로 viz 막대 갱신**(B-3-a).
3. `plat_music_get`는 이제 **실제 `music_snapshot_get`**(music_events.c)을 호출하도록 바꾼다.

#### B-3-a. viz 스펙트럼 (fft_map 대체)
- `fft_map.c`(esp_dsp) 대신 **sim 자체 실수 FFT**로 `audio_viz_snapshot_t{bars[256],peaks[256],level}`를 채운다.
  구현은 `sim/sim_audio.c`(신규) 또는 `platform_sim.c` 내부 어디든 무방.
- 사양(근사면 충분): 256-pt 윈도우(Hann) → 크기 스펙트럼 → 로그 매핑으로 `VIZ_POINTS(256)` 빈에 분배 →
  정규화(대략 `fft_map_db_to_norm`류 dB→0..1) → 피크 감쇠(`peaks[i]*=0.96f`, `if(v>peaks[i])peaks[i]=v`).
  `level`은 B-3의 `level` 사용. **품질은 "소리에 확실히 반응"이면 됨**(가젯 정확도까지 맞출 필요 없음).
- `AUDIO_TUNER` 모드에서는 막대를 갱신하지 않고 자연 감쇠(또는 0)로 둔다(가젯도 튜너 모드에선 fft를 안 돌림).
- `plat_audio_viz_get`/`audio_viz_snapshot_get`가 이 실측 스냅샷을 반환.

### B-4. 폴백 (장치 없음/열기 실패)
- 캡처 장치를 못 열면 **기존 합성 경로로 폴백**(현재 sim 동작 보존): 합성 사인 스윕 viz + 합성 피치(마우스 X) +
  `O` 키 수동 온셋. 시작 시 `W (sim) no capture device — synthetic audio fallback` 류 경고 1줄 출력.
- 이때도 Part A 키맵(Backspace/Space/Enter/화살표)은 동일하게 동작해야 한다.

### B-5. 초기화
- `plat_init`에서 `tuner_init()` + `music_events_init()`를 1회 호출(가젯 `audio_task` 초기화 미러링).
  캡처 장치 오픈/`SDL_PauseAudioDevice(dev,0)`도 여기서.

---

## 불변조건
- **ESP 빌드 무회귀.** `tuner.c`/`music_events.c`/`audio_config.h`/UI 소스 **일체 수정 금지**(sim 빌드에 컴파일만 추가).
- **UI 파일 무수정**(분기는 `platform_*`/`sim/`에만). `#ifdef` 남발 금지.
- **DSP 동일성:** 피치/온셋/BPM은 가젯과 **같은 `tuner.c`+`music_events.c`** 로 계산(sim 자체 재구현 금지). viz FFT만 sim 로컬.
- **블록 규격 동일:** 256 샘플, `raw/2^31` 정규화 스케일 감각(캡처는 F32이므로 이미 −1..1), `rms`/`level` 식 app_main.c와 일치.
- **스텁 중복 정의 0:** `tuner_*`·`music_snapshot_get`·`synth_music_snapshot`를 `platform_sim.c`에서 제거했는지 확인.
- 경고: MSVC `/W4`·비-MSVC `-Wall -Wextra`에서 신규 경고 0. (`tuner.c`/`music_events.c`는 `stdatomic.h` 사용 → MSVC C11 atomics로 컴파일되는지 확인; 문제 시 sim 빌드에 `/std:c11` 명시.)

## 완료 판정
- **입력:** ↑↓←→ 이동, **Enter=OK**, **Backspace=HOME**(길게=런처 복귀 HOME_HOLD), **Space=FOOTSW**(짧게=다음 앱, 길게=튜너 점프).
- **실오디오:** 스테레오 믹스 또는 마이크 선택 시 —
  - **Tuner** 앱이 실제 연주/재생 음의 음정을 표시(센트 반응),
  - **Sound Monitor** 스펙트럼이 실제 소리에 반응,
  - **Bounce**가 실제 온셋마다 점프(합성 Space 주입 없이).
- **폴백:** 캡처 장치 없을 때 합성 경로로 동작 + 경고 로그, 키맵은 동일.
- `sim_nvs.bin`으로 재실행 후 런처/테마/슬롯 상태 유지(기존 동작 무회귀).
- ESP `idf.py build` 무회귀(-Werror).

## 산출물
- `sim/platform_sim.c` diff(키맵 변경 + 스텁 제거 + 캡처/펌프 + plat_music/viz 연결),
- (있으면) `sim/sim_audio.c`,
- `sim/UI_SOURCES.cmake`(+ `tuner.c`/`music_events.c`),
- `sim/CMakeLists.txt` 변경(있으면),
- `sim/README.md`(키맵 표 + 오디오 장치/스테레오믹스 사용법 + `--list-audio`/`--audio-device`),
- 링크 충돌 제거 확인 메모(어떤 스텁을 지웠는지).
