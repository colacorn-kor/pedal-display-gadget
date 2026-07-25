# Codex 지시서 — 저장소 위생(긴급) · 오디오 크래시 · TRS · 반응성 · 문서

> **건⓪(저장소 위생)이 최우선이며 다른 모든 것의 전제다.** 커밋된 `sdkconfig`/`build/`가
> 하드웨어 설정(CPU 240→160MHz, LVGL PSRAM 버퍼 해제, O3→Og)을 덮어써 **현재 부팅 시 화면이
> 뜨지 않는다**(display init 실패). 이걸 먼저 고치지 않으면 나머지 수정도 검증 불가다.
> 공통 불변조건: `-Werror` 통과, 호스트 테스트 3/3, sim 빌드 무회귀, UI/앱/렌더러 로직 수정 0.

---

# 건⓪ 저장소 위생 — 빌드 산출물/설정 오염 제거 (최우선)

## 확인된 문제
실기 부팅 로그:
```
E LVGL: Not enough memory for LVGL buffer (buf2) allocation!
E display: lvgl_port_add_disp failed
E app: display initialization failed
I cpu_start: cpu freq: 160000000 Hz         ← 240이어야 함
-- USING O3                                  ← CMake는 O3 원함
CONFIG_COMPILER_OPTIMIZATION_DEBUG=y         ← 그러나 sdkconfig가 Og로 덮음
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160          ← 160으로 고정됨
```
원인: `sdkconfig`, `sdkconfig.old`, `build/`(2420개 추적), `managed_components/`(5351개 추적)가
git에 커밋되어 있고 **`.gitignore`가 없다.** Codex/CI가 자기 환경에서 빌드하면 그 환경의
`sdkconfig`가 커밋되어 태윤의 하드웨어 설정을 덮어쓴다. 이번 회귀의 직접 원인이며,
방치하면 매 빌드마다 재발한다.

## ⓪-A `.gitignore` 신설
저장소 루트에 `.gitignore`를 만든다. 최소 항목:
```
/build/
/sdkconfig
/sdkconfig.old
/managed_components/
*.pyc
__pycache__/
```
- `managed_components/`는 `idf_component.yml`/lock으로 재현되므로 추적하지 않는다.
- `sdkconfig.defaults`는 **추적 유지**(소스 오브 트루스). `sdkconfig`(파생물)만 제외한다.

## ⓪-B git 추적에서 제거 (파일은 로컬 보존)
```
git rm -r --cached build managed_components
git rm --cached sdkconfig sdkconfig.old
```
- `--cached`로 추적만 해제하고 워킹트리 파일은 남긴다(태윤의 로컬 빌드 보존).
- 커밋 메시지에 "빌드 산출물·파생 sdkconfig를 추적에서 제거, .gitignore 추가" 명시.

## ⓪-C `sdkconfig.defaults`에 하드웨어 필수 설정을 명시적으로 박기
현재 `sdkconfig.defaults`는 CPU 주파수·최적화·WDT를 지정하지 않아, 파생 `sdkconfig`가 없으면
IDF 기본값(160MHz, Og 아님이지만 불명확)으로 떨어진다. 아래를 **추가**한다:
```
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```
- **CPU 240MHz**: 튜너 `analyze()` 실시간성의 전제. 160MHz면 건①을 고쳐도 마진이 부족하다.
- **O3(PERF)**: CMake의 `-- USING O3`와 일치시킨다. 현재 sdkconfig의 Og와 충돌 중.
- **PSRAM 80MHz + malloc**: LVGL 더블버퍼(480×320×2×2 ≈ 600KB)가 PSRAM 힙에 잡히도록.
  현재 40MHz로 떨어져 있어 대역폭도 절반이다.
- ⚠ 이 값들이 실제 sdkconfig에 반영되려면 태윤이 **`build/` 삭제 후 재구성**해야 한다.
  지시서 말미 "태윤 적용 절차"에 명시할 것.

## ⓪-D LVGL 버퍼 할당 경로 점검 (코드)
`sdkconfig`를 고쳐도 LVGL 버퍼가 **내부 RAM 우선**으로 잡히면 다시 실패할 수 있다.
`display` 초기화(`lvgl_port_add_disp` 호출부)에서 버퍼가 **PSRAM(`MALLOC_CAP_SPIRAM`)**
또는 `buffer_size`가 과대하지 않은지 확인한다:
- `esp_lvgl_port`의 `buffer_size`와 `double_buffer` 설정을 확인.
- 더블버퍼가 내부 RAM만 요구하면, 단일 버퍼로 낮추거나 PSRAM 사용 플래그를 켠다.
- **단, 현재 코드는 이전 빌드에서 화면이 정상 동작했다.** 따라서 근본 원인은 sdkconfig(⓪-C)일
  가능성이 높다. ⓪-C 적용 후에도 buf2 실패가 나면 그때 이 항목을 손댄다. 먼저 sdkconfig를
  고치고, 코드는 필요할 때만 최소 변경.

## 건⓪ 완료 판정
- `git ls-files`에 `build/`·`managed_components/`·`sdkconfig`·`sdkconfig.old` 없음.
- `.gitignore` 존재, 위 항목 무시.
- `sdkconfig.defaults`에 CPU 240 / O3 / PSRAM 80M 명시.
- (태윤이 build 재구성 후) 부팅 로그에 `cpu freq: 240000000 Hz`, display init 성공, 화면 표시.

---

# 건① 오디오 태스크 폭주 → 워치독 (⓪ 다음)

## 원인 (240MHz 기준 재계산)
- `analyze()`: tau 5~268 내부 루프 369,468회. **O3·240MHz에서 약 9~12ms**
  (현재 오염 상태 Og·160MHz에서는 25~35ms로 훨씬 악화 — 건⓪가 이걸 정상화한다).
- 실행 주기 `HOP 192 @ 8kHz` = 24ms.
- I2S DMA 버퍼 `dma_desc_num=4 × 256` = 21.3ms.
- 오버플로 시작 → `i2s_channel_read` 즉시 반환 → 우선순위 6 audio_task 무한 회전 →
  IDLE1 굶주림 → 워치독. 오버플로 로그가 매 카운트 발행되어 `uart_write`에서 악화.

## ①-A 오버플로 로그 레이트 제한
- `app_main.c` 292행 부근. **최소 2000ms 간격**. 마지막 로그 시각 정적 변수.
- 누적/증가분 동시 출력: `"I2S RX overflow: total=%u (+%u in last %ums)"`.
- `"I2S returned an invalid byte count"` 로그도 2초 제한.

## ①-B DMA 버퍼 증설
- `chan_cfg.dma_desc_num = 4` → **8** (21.3ms → 42.6ms).

## ①-C analyze 빈도 완화
- `tuner.c` `#define HOP 192` → **384** (주기 24→48ms).
- ⚠ sim 공유 파일. **상수 1개만** 변경, `_Static_assert` 통과 확인.

## ①-D 워치독 안전망
- `audio_task` 루프에 **64블록마다 `vTaskDelay(1)`**.

## 건① 완료 판정
- 튜너 앱 5분 이상 워치독 없음. 오버플로 로그 2초 1줄 이하. 다른 앱 무변경.

---

# 건② TRS 래치 안정성

## ②-A
- 후보 상태 **연속 3회 동일** 시에만 래치 갱신. `#define INPUT_TRS_STABLE_COUNT 3`.
- 기존 전이 규칙 4가지 유지(래치없음+유효→래치 / 래치+idle→해제 / 더높은비율→전환 / 더낮음·데드존→무시).
- 안정성 카운터는 래치 갱신에만. 디바운스/홀드/리피트 상태머신 수정 금지.

---

# 건③ 판정표 재보정 (Rtop 10k 신회로)

```
Ring(+3V3)─[Rtop 10k]─┬─Tip─→G4    UP=0Ω DOWN=470Ω LEFT=1k RIGHT=2k OK=4.7k HOME=10k
```
`app_main.c`의 CENTER/WINDOW 상수를 교체:

| 키 | CENTER | WINDOW | @3208mV |
|---|---|---|---|
| UP | 0.0051 | 0.0117 | 0~54 |
| DOWN | 0.0505 | 0.0096 | 131~193 |
| LEFT | 0.0970 | 0.0101 | 279~344 |
| RIGHT | 0.1736 | 0.0387 | 433~681 |
| OK | 0.3285 | 0.0400 | 926~1182 |
| HOME | 0.5113 | 0.0400 | 1512~1769 |
| IDLE | ratio ≥ 0.85 | — | ≥2727 |

- 57조합 2키 오인식 0건. 유래 주석 남길 것. `INPUT_TRS_LOG=1` 유지.

---

# 건④ 반응성 (최소 범위 — 공정한 평가용)
현재 래더 지연 폴링10+안정성30+디바운스30 ≈ 70ms. 목표 ~30ms.
- ④-A `INPUT_POLL_MS` 10 → **5**. ADC 32평균이 5ms 초과 시 16회로 낮추고 노이즈 로그 보고.
- ④-B `input_button_t`에 `debounce_ms` 필드 추가. **래더 6키=10ms, FOOTSW=30ms 유지.**
- ⚠ `INPUT_HOLD_MS`/`REPEAT_DELAY`/`REPEAT_RATE` 변경 금지.

---

# 건⑤ 문서 동기화 (SSOT)
- ⑤-A `hardware/NETLIST_SPEC.md §7`: Rtop 4.7k→**10k**, 저항값 신회로로, `TRS_SIG`(ADC겸디지털),
  Tip 직렬 220Ω, Ring 직렬 100Ω, **Ring +3V3 고정·5V 금지** 명시.
- ⑤-B `ASSEMBLY.md`: 저항값 신회로 + 220Ω/100Ω + "TS 플러그 삽입 시 Ring 단락" 주의.
- ⑤-C `PROJECT_MASTER.md`: 확장 트랙에 **S6 스마트 컨트롤러(MCU 6키)** 신설, `CONTROLLER_DESIGN.md`
  참조. 문서 지도(§0)에 `CONTROLLER_DESIGN.md` 등재. (문서 본체는 태윤이 추가, Codex는 만들지 말 것.)
- ⑤-D `PUNCHLIST.md`: I2S overflow 항목 건①로 해결 처리. **신규 항목 추가**: "sdkconfig/build
  추적 제거 완료(건⓪)".

---

## 불변조건 (전체)
- `ui_event_t`·`EV_*`·`renderer_t`·`gadget_app_t` 인터페이스 변경 0.
- `INPUT_HOLD_MS`/`REPEAT_DELAY`/`REPEAT_RATE` 변경 0.
- `tuner.c`는 `HOP` 1개만. 알고리즘·다른 상수·필터 무변경.
- `music_events.c`·`fft_map.c`·오디오 정규화·rms/level 식 무변경.
- UI/앱/렌더러 파일 수정 0. `INPUT_TRS_LADDER=0` 컴파일 성공.
- `input_button_read_raw()` 캡슐화 유지(Smart 전환 지점).
- sim 빌드 무회귀.

## 완료 판정
1. **부팅 시 화면 표시 + `cpu freq: 240000000 Hz`** (건⓪ 핵심).
2. `git ls-files`에 build/sdkconfig/managed_components 없음.
3. 튜너 5분 연속 워치독 없음.
4. 6키 정확 인식, 특히 UP 안정. UP 연타 시 오입력·오전환 없음.
5. latch가 처음부터 올바른 키(HOME/OK 경유 없음).
6. RIGHT+OK 동시 → 이벤트 없음. 키 유지 중 추가 → 처음 키 유지.
7. 방향키 오토리피트·HOME 롱프레스·FOOTSW 단/장 정상. 이동 체감 개선.

## 산출물
- `.gitignore`, `git rm --cached` 결과, `sdkconfig.defaults` diff.
- `app_main.c`·`tuner.c` diff. 240MHz·O3 기준 `analyze()` 부하 재계산.
- 문서 diff: NETLIST_SPEC / ASSEMBLY / PROJECT_MASTER / PUNCHLIST.

## ⚠ 태윤 적용 절차 (Codex 커밋 수령 후)
`sdkconfig.defaults`가 실제로 반영되려면 **기존 파생 설정을 지우고 재구성**해야 한다:
```powershell
cd $HOME\Documents\GitHub\pedal-display-gadget
Remove-Item -Recurse -Force build, sdkconfig, sdkconfig.old -ErrorAction SilentlyContinue
idf.py set-target esp32s3
idf.py build
idf.py -p COM4 flash monitor
```
부팅 로그에서 `cpu freq: 240000000 Hz`와 화면 표시를 먼저 확인한 뒤 나머지 판정으로 진행.
