# CLAUDE_HANDOFF.md — GUI/GG 프로젝트 완전 인수인계

> **인수 완료 메모(2026-07-26):** 이 문서는 `4e098a5` 시점의 역사적 스냅샷이다.
> 현재 상태는 `LAB_STATE.md`, 지속 규칙은 `AGENTS.md`를 따른다. 인수 감사에서
> 등록 앱은 5개가 아니라 4개로 확인됐고, input WDT의 직접 원인은 FreeRTOS 100Hz에서
> 5ms 지연이 0 tick이 된 것이었다. 원 지시서의 32탭·320ms 이동평균은 키 창을 순차 통과해
> 오인 래치를 만들 수 있어 채택하지 않았다.
>
> 작성: Claude (Anthropic) → 차기 담당 에이전트
> 기준 시점: 2026-07-26, 저장소 HEAD = `4e098a5`
> ⚠ **§7과 §11을 먼저 읽을 것.** 미적용 핫픽스가 있으며, 그것이 현재 장치를 사용 불가로 만들고 있다.

---

## 1. 제품 목표와 현재 단계

**제품:** GUI/GG (Guitar User Interface / Graphic Guitar). 기타 페달보드용 미니 디스플레이 가젯.
9V 전원, 팔뚝 크기. 실시간 튜너 · 스펙트럼/반응형 비주얼라이저 · 이미지 표시 · 출력 뮤트.
철학: 실용성 + 창작 자유(테마, 핫스왑 키캡, 이스터에그). **초보자가 마주치자마자 plug-and-play**
되어야 하며, MIDI 같은 고급 기능은 별도 확장 출입구로 분리한다.

**현재 단계:** 브레드보드 프로토타입. 펌웨어는 앱 플랫폼(런처·테마·5개 앱) 완성,
PC 시뮬레이터(실오디오 입력 포함) 완성, TRS 6키 저항 래더 입력 구현 완료.
**단, 마지막 플래시에서 input 태스크 워치독 리셋로 장치가 실사용 불가 상태(§7).**

**개발 소프트웨어 스택:** ESP-IDF v5.4.4 / LVGL 9.5.0 / esp_lvgl_port 2.8.0 / esp-dsp 1.8.2 /
esp_lcd_st7796 1.4.0 / KiCad 9 / Windows, 포트 COM4 / 버전관리 GitHub Desktop.

**협업 모델(중요):** 사용자(태윤)가 방향 결정 → AI가 실제 코드를 검토하고 `CODEX_INSTRUCTION_*.md`
지시서 작성 → Codex가 구현·커밋 → AI가 불변조건 대비 검토 → 태윤이 실기 플래시·검증.
**AI는 저장소에 코드를 직접 쓰지 않는다.** 작업 언어는 한국어.

---

## 2. 문서 권위와 우선순위

| 영역 | SSOT | 비고 |
|---|---|---|
| 총괄 로드맵·확장 트랙 | `PROJECT_MASTER.md` | §0에 문서 지도 |
| 펌웨어 구조 | `ARCHITECTURE.md` | |
| 하드웨어 넷 연결 | `hardware/NETLIST_SPEC.md` | **핀맵·저항값의 최종 권위** |
| 브레드보드 물리 조립 | `ASSEMBLY.md` | 물리 작업은 이 문서만 따름 |
| 외부 컨트롤러 전략 | `CONTROLLER_DESIGN.md` | 2단(Basic/Smart) 전략 + 인터페이스 계약 |
| 런처/UI | `LAUNCHER_DESIGN.md`, `UI_DESIGN.md` | |
| 미결 항목 | `PUNCHLIST.md` | |

원칙: **코드가 바뀌면 즉시 문서 동기화.** 문서-코드 불일치는 다음 세션에서 환각을 유발한다
(실제 사고 이력 있음). 충돌 시 실기 로그 > 코드 > 문서 순으로 신뢰하되, 불일치 발견 즉시 교정.

`CODEX_INSTRUCTION_*.md`는 1회용이다. 적용 완료된 지시서는 삭제해 왔다. 현재 저장소의
`CODEX_INSTRUCTION_audio_crash_ladder_fix.md`·`CODEX_INSTRUCTION_repo_hygiene_and_fixes.md`는
**적용 완료본이므로 삭제 대상**(태윤 확인 후).

---

## 3. 확정된 설계 결정과 근거

### 3-1. 저장소·빌드 (커밋 4e098a5로 확정)
- `.gitignore`로 `build/`·`managed_components/`·파생 `sdkconfig*` 추적 금지. **근거:** Codex가
  자기 샌드박스에서 빌드한 sdkconfig(160MHz·Og·PSRAM40M)가 커밋되어 태윤 하드웨어 설정을
  덮어썼고, 화면 부팅 실패를 일으켰다(실제 사고, 2026-07-25).
- `sdkconfig.defaults`에 명시: **CPU 240MHz / COMPILER_OPTIMIZATION_PERF / SPIRAM 80MHz OCT
  / SPIRAM_USE_MALLOC**. defaults에 없으면 IDF 기본 160MHz로 떨어진다.
- 참고(Codex 검증): IDF 5.4.4에서 `PERF`는 일반 소스에 **-O2**를 적용, `USING O3`는 esp-dsp만.

### 3-2. 오디오/튜너 (적용 완료, 실기 부분 검증)
- I2S `dma_desc_num=8` (버퍼 42.6ms), 오버플로 로그 2초 레이트 제한(누적+증가분),
  audio_task 64블록마다 `vTaskDelay(1)` 안전망.
- `tuner.c` `HOP 192→384` (analyze 주기 24→48ms). **근거:** analyze()는 내부 369,468회 반복
  ≈ 240MHz·O2에서 9~12ms. HOP 192에서는 CPU1 점유 과다로 I2S 오버플로→무한회전→워치독
  (실기 재현됨). tuner.c는 sim과 공유하므로 **HOP 외 다른 상수·알고리즘 변경 금지.**

### 3-3. TRS 6키 저항 래더 (Basic 컨트롤러)
- **회로(확정, 조립 완료):** Ring(+3V3) ─ Rtop **10k** ─ Tip → G4(ADC1_CH3).
  키 저항(Tip↔Sleeve): UP=0Ω직결 / DOWN=470 / LEFT=1k / RIGHT=2k / OK=4.7k / HOME=10k.
- **근거(1차 회로 폐기 이력):** 최초 Rtop 4.7k + 0/150/470/1k/2k/10k는 DMM 검증까지 통과했으나,
  실기에서 **접점 저항(택트+브레드보드 0~100Ω)** 이 저저항 키를 흔들어 UP이 전혀 인식되지
  않았다(실측: UP이 23~65mV로 부유). Rtop을 키워 접점 저항 비중을 낮춘 것이 현 회로.
  UP↔DOWN 간격 32mV→112mV.
- **판정 방식(타협 불가 2원칙):**
  1) **최근접(nearest) 금지, 창(window)+데드존만.** 근거: RIGHT+OK 동시 입력이 최근접에서는
     LEFT로 오인식된다(계산+실측 405mV로 확인).
  2) **절대 mV가 아닌 비율(ratio = 읽기/idle_ref).** idle_ref는 무입력 시 느린 시정수로 자동
     추적. 근거: 공급·ADC 기준 드리프트 상쇄. 실기에서 idle_ref 3208~3210mV로 안정 동작 확인.
- **판정표(현 코드 반영, Rtop10k 이론+구회로 ADC특성 외삽 잠정값):**
  UP 0.0051±0.0117 / DOWN 0.0505±0.0096 / LEFT 0.0970±0.0101 / RIGHT 0.1736±0.0387 /
  OK 0.3285±0.0400 / HOME 0.5113±0.0400 / IDLE ≥0.85.
  ⚠ **신회로 실측 재보정 미완**(§9-2). 구회로 실측에서 ADC가 DMM 대비 오프셋 +30mV,
  게인 +3.3% 상향 편의를 보였다 — 신회로에서도 유사 편의 예상.
- **래치 정책:** 물리 근거 "키 추가 시 전압은 반드시 하강" → ①래치없음+유효키→래치
  ②idle→해제 ③더 높은 비율 키→전환 ④더 낮음/데드존→무시. 여기에 **연속 3회 안정성
  조건**(INPUT_TRS_STABLE_COUNT) 추가. 근거: 무입력→키 하강 스윕 중 32회 평균이 과도값을
  만들어 HOME/OK가 오래치되는 버그 실기 재현(예: LEFT 눌렀는데 latch=HOME 고착).
- 기존 6개 GPIO 택트 스위치(G4/5/6/7/15/16)는 **물리 제거됨**. `INPUT_TRS_LADDER=1` 기본,
  0은 컴파일만 보장(롤백 참조용).
- 입력 이벤트 레이어(`input_button_t` 디바운스/홀드/리피트)는 **무수정 재사용** —
  `input_button_read_raw()`가 raw level 출처를 캡슐화(래더 latch → 합성 active-low).
  **이 캡슐화가 Smart 컨트롤러 전환 지점이므로 반드시 유지.**

### 3-4. 외부 컨트롤러 2단 전략 (`CONTROLLER_DESIGN.md`, 대화에서 확정)
- **Basic(저항 래더) = 보급형 라인, Smart(MCU+오픈드레인 1-wire) = Phase 2.** 마샬
  PEDL-91016(TS 1선, 앰프가 전원 급전, MCU 내장 시리얼) 방식을 참조 모델로 조사했음.
- **물리 인터페이스 계약(위반 금지):**
  - **Ring = +3V3 고정, 5V 절대 금지.** 근거: 5V Ring에 Basic을 꽂으면 무입력 5V가 G4로
    들어가 ADC 절대최대 초과, 핀 파손. 태윤도 "래더에 5V 불필요" 동의.
  - Rtop 10k는 Smart에서 오픈드레인 풀업 겸용 — 전환 시 제거하지 않음.
  - Tip 직렬 220Ω(Smart 고장 전류 제한, ADC 영향 없음), Ring 직렬 100Ω(TS 플러그 오삽입 시
    3V3 단락을 33mA로 제한 — TS 패치케이블 혼용 환경에서 오삽입은 필연).
    ⚠ **이 220Ω/100Ω은 문서에는 반영됐으나 브레드보드 실물 장착 여부 미확인**(§11-4).
  - G4는 "ADC 겸 디지털 겸용" — PCB에서 RC 필터 100nF 이하, 넷 이름 `TRS_SIG`.
- **Smart 전환 판단 기준:** 예측이 아닌 **실사용 체감**(래더 반응성 불쾌 여부, 다중키 필요 여부).
  부팅 시 500ms hello 패킷 청취로 Basic/Smart 자동 판별 예정(래더는 정적 DC라 오인 불가).

### 3-5. 반응성 (건④ — ⚠ 부분 실패, §7 참조)
- 버튼별 `debounce_ms` 도입: 래더 6키 10ms(안정성 카운터가 이미 필터), FOOTSW 30ms(기계 접점).
- `INPUT_POLL_MS 10→5`는 **워치독 사고를 유발**해 핫픽스에서 10ms 복귀 예정.
- `INPUT_HOLD_MS 500 / REPEAT_DELAY 400 / REPEAT_RATE 120`은 사용자 체감 타이밍 — 변경 금지.

---

## 4. 검토 후 폐기한 대안 (재제안 금지)

| 대안 | 폐기 이유 |
|---|---|
| 6키를 MIDI로 대체 | MIDI IN은 전원 급전 불가(별도 어댑터 필요), LED엔 역방향 케이블 추가 필요, 초보 진입장벽. 태윤 명시 거부. MIDI는 프로용 확장 출입구로만(Phase 2 S4). |
| 4선/I2C(MCP23017) 컨트롤러 | 기술적으론 우수(원격 펌웨어 불요)하나 **페달보드에서 4선 케이블이 부자연스러움**. 태윤 명시 거부. |
| USB 계열 케이블 | 페달보드 미관. 태윤 거부. |
| 다중키 이진가중 래더(R/2/4/8...) | 64단계가 좁은 전압범위에 압축되어 저항 오차·노이즈에 취약. 6키에 과설계. |
| 증분 컨덕턴스 디코딩(last-wins) | 오차 누적 전파, 3키 이상 불안정. Smart로 해결이 정도. |
| 래더 판정 최근접(nearest) 방식 | RIGHT+OK→LEFT 오인식(§3-3). **절대 금지.** |
| 1차 래더 회로(Rtop4.7k, DOWN=150Ω 등) | 접점 저항에 취약, UP 미인식 실기 확인. |
| Codex의 sdkconfig 커밋 유지 | 환경 오염 사고 원인. `.gitignore`로 봉인 완료. |
| 반응성 목표 30ms(폴 5ms) | CPU0 과점유로 워치독·화면 정지 유발. 55ms(폴 10ms+이동평균)로 재설정. |

---

## 5. 하드웨어 — 모델·핀맵·전기 금지사항

**부품:** ESP32-S3-DevKitC-1 N16R8(16MB flash/8MB PSRAM OCT) · ST7796S 3.5" 480×320 SPI ·
PCM1808 I2S ADC(+5V from MP1584, 3.3V from DevKit) · TL072CP 입력 버퍼(패스스루) ·
MP1584 벅 + 1N5819 역극성 보호.

**확정 핀맵(NETLIST_SPEC.md가 권위):**
- LCD: BL=G1 CS=G2 DC=G21 RST=G14 SCL=G12 SDA=G13 (INVERT=0 RGB_BGR=1 SWAP_XY=1
  MIRROR_X=0 MIRROR_Y=0 GAP=0 SWAP_BYTES=1 PCLK=40MHz)
- I2S: MCLK=G8 BCK=G9 DIN=G10 WS=G18
- TRS 래더: Tip→G4(ADC1_CH3), Ring=+3V3, Sleeve=GND. FOOTSW=G17(내부풀업 active-low)
- 뮤트=G3 (J201 게이트 드라이브 **미설계** — 직결 금지)
- SD: MISO=G11 CS=G47 (SPI 버스 G12/G13 공유, 펌웨어 스텁만. VCC=+3V3 직결, 순수 어댑터 전제)
- G5/G6/G7/G15/G16: 미사용(Phase 2 예비). G40/41/42: Phase 2 코덱 예약.

**전기 금지사항(사고 이력 포함):**
1. **USB 플래시 중 외부 9V 동시 연결 금지** (백피드).
2. **TRS Ring에 5V 인가 금지** (§3-4, G4 파손).
3. 뮤트 J201에 G3 직결 금지 (음전압 게이트 드라이브 필요, 미설계).
4. 9V 인가 전 **모든 GND 본딩 확인** (플로팅 유령전압 사고 이력).
5. 단일공급 op-amp Rg는 GND가 아닌 VREF(4.5V)에 (출력 포화 방지).
6. ESP32 classic 데이터시트의 GPIO 제약을 S3에 적용하지 말 것. S3는 BT Classic 미지원
   (오디오 스트리밍 불가, BLE-MIDI/WiFi는 가능).
7. AMS1117/레벨시프터 붙은 SD 모듈 비권장 — 40MHz LCD 공유 SPI 버스를 오염시킴.

---

## 6. 확인된 정상 동작 (실기 검증 완료)

- 4개 앱(monitor/images/tuner/bounce) + 런처·테마 화면 동작 (구 GPIO 버튼 시절 + 이후).
- 부팅 체인: 240MHz / PSRAM 80MHz / display init / `boot complete` / ADC curve-fitting 캘리 성공
  (2026-07-26 마지막 플래시 로그).
- 래더 ratio 판정·idle_ref 자동추적: IDLE에서 3208~3210mV로 견고. 구회로에서 6키+RIGHT+OK
  동시입력까지 이론과 1% 이내 일치 확인.
- 래치 "키 유지 중 추가 키 무시"(RIGHT 유지+OK 추가) 정상 동작 확인(구회로 로그).
- PC 시뮬레이터: 실오디오 캡처(SDL2, 48k/256블록), 공유 tuner.c/music_events.c로 하드웨어와
  동일 DSP. 키맵 ↑↓←→/Enter=OK/Backspace=HOME/Space=FOOTSW/O=합성온셋/Esc종료.
  `--list-audio`/`--audio-device N`. 렌더러 캐시 최적화 후 시각 무회귀 육안 확인(Codex).
- 오디오 입력은 **아직 미연결**(주변 노이즈로 앱 동작만 확인하던 단계). TL072 프론트엔드
  연결이 다음 하드웨어 마일스톤.

---

## 7. 마지막 플래시 결과와 미해결 증상 ⚠ 최우선

**2026-07-26 플래시(커밋 4e098a5 + 로컬), 부팅은 전부 성공했으나:**
```
E task_wdt: IDLE0 (CPU 0)  /  CPU 0: input
adc_oneshot_read → input_ladder_read_average(app_main.c:534) → input_task(:818)
```
- **5초마다 워치독 리셋 반복. 화면은 정적 노이즈로 정지, 버튼 무반응.**
- 원인(확정): CPU0에 display(prio4)+input(prio5)+LVGL+IDLE0이 몰린 배치에서, input이
  5ms 폴마다 `adc_oneshot_read` **32회 블로킹 루프**를 돌아 display·IDLE0를 굶김.
- **핫픽스 지시서 `CODEX_INSTRUCTION_input_wdt_hotfix.md` 작성 완료, 아직 미적용/미커밋.**
  내용: ①ADC 폴당 1회 + 32탭 이동평균(링버퍼, 워밍업 처리) ②input 우선순위 5→3
  ③`INPUT_POLL_MS` 5→10 복귀(+hold/repeat 실시간 값 불변 확인).
  이 파일은 태윤의 로컬 다운로드에 있으며 저장소에는 없다 — **차기 담당자는 이 핫픽스를
  최우선으로 적용시켜야 한다.** (지시서가 유실됐다면 위 3항을 지시서로 재작성.)

미검증으로 남은 실기 항목: 신회로 6키 인식(특히 UP), UP 연타 무오입력, latch 무경유 직행,
RIGHT+OK 데드존, 홀드/리피트, 튜너 5분 무리셋, 래더 반응성 체감.

---

## 8. 빌드·플래시·모니터 절차

ESP-IDF 5.4 PowerShell(또는 `. $HOME\esp\v5.4.4\esp-idf\export.ps1`)에서:
```powershell
cd $HOME\Documents\GitHub\pedal-display-gadget
idf.py -p COM4 flash monitor            # 통상
idf.py -p COM4 monitor | Tee-Object -FilePath log.txt   # 로그 파일 수집
# 종료: Ctrl+]
```
**sdkconfig.defaults가 바뀐 커밋을 받은 뒤에는 반드시:**
```powershell
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
Remove-Item -Force sdkconfig,sdkconfig.old -ErrorAction SilentlyContinue
idf.py set-target esp32s3 ; idf.py build
```
(파생 sdkconfig가 defaults를 덮으므로 — 이 절차 누락이 실제 사고 원인이었다.)

호스트 테스트: `tests/` 3종(midi_parser/tuner_detection/fft_normalization) — Codex가 빌드마다
실행. PC 시뮬레이터: `sim/` CMake+MSVC, 산출물 `pedal_sim.exe`.

---

## 9. 남은 작업 (우선순위순)

1. **[긴급] input 워치독 핫픽스 적용**(§7) → 플래시 → 화면·버튼 소생 확인.
2. **신회로 판정표 실측 재보정:** `INPUT_TRS_LOG=1` 로그로 7상태(6키+IDLE) 수집, 특히
   **UP/DOWN 분리 간격**과 ADC 편의 확인 → 판정표를 실측 중심으로 확정 → 로그 0으로.
3. 래더 반응성 실사용 체감 평가 → **Smart 컨트롤러 착수 여부 결정**(CONTROLLER_DESIGN §6).
4. 적용 완료된 CODEX_INSTRUCTION 2건 삭제(태윤 확인 후).
5. Ring 100Ω / Tip 220Ω 실물 장착 확인(§11-4) 및 미장착 시 장착.
6. 오디오 입력(TL072 프론트엔드) 연결 + 노이즈 저감 — 다음 하드웨어 마일스톤.
   이후 튜너 5분 무리셋 실기 검증(§3-2의 완결).
7. PUNCHLIST 잔여: 앰프 게인 튜닝, 온셋 상수 튜닝(실오디오 연결 후에만 가능).
8. KiCad: 스키매틱 완성 → .net 내보내기 → NETLIST_SPEC diff 루프. J201 게이트 드라이브 설계.
9. Phase 2 트랙(PROJECT_MASTER S1~S6): 코덱 출력, MIDI(TRS-A, 옵토), WiFi/OTA, BLE-MIDI,
   스크립트 로더, Smart 컨트롤러.

---

## 10. 사용자(태윤)와의 작업 규칙

1. **세션 시작 시 저장소를 실제로 읽고 시작한다. 추측 금지.** public repo는 클론 가능.
2. 지시서에는 **명시적 불변조건 목록**을 넣고, 구현 후 항목별 통과를 검증한다.
3. Codex 보고를 그대로 믿지 않는다 — **푸시된 코드를 diff로 재검증**한다(파일 단위 바이트
   비교 포함). Codex는 유능하지만 하드웨어가 없어 실기 검증을 못 한다.
4. 실기 검증은 태윤 담당. 로그 수집 절차(어느 키를 몇 초, 어떤 순서)까지 구체적으로 안내한다.
5. 수치 주장은 계산으로 뒷받침한다(전압 분배·CPU 부하·타이밍 등 — 본 프로젝트에서 계산
   검증이 실제 버그 3건을 사전/사후 적발했다).
6. **타이밍·우선순위를 바꾸는 지시는 코어별 태스크 배치와 부하를 먼저 계산한다.**
   (건④에서 이를 생략해 워치독 사고를 냈다 — 같은 실수 반복 금지.)
7. 자신의 지시가 사고를 냈으면 그렇게 말한다. 태윤은 근거를 갖춘 교정을 신뢰한다.
8. 태윤의 제품 감각(TS 케이블 선호, 페달보드 미관, 초보 UX)은 설계 제약으로 존중한다.
   기술 최적해가 제품 결정을 이기지 않는다.
9. 문서 위상 안내: 물리 작업 질문 → ASSEMBLY.md 하나로 답한다. 새 결정은 즉시 SSOT에 반영.
10. 한국어로 작업한다. 지시서·문서도 한국어.

---

## 11. 차기 담당자가 가장 먼저 검증할 위험 요소

1. **[사실] input 워치독(§7).** 핫픽스 미적용 상태로는 어떤 실기 검증도 불가능하다.
   적용 후 "워치독 무발생 + 정상 UI + 버튼 반응"을 확인하기 전까지 다른 작업 금지.
2. **[사실] UP/DOWN ADC 분리(신회로 미실측).** 구회로에서 접점 저항이 UP을 죽였다.
   신회로 이론상 간격 112mV이나 실측 전이다. 로그에서 UP 최대값과 DOWN 최소값 간격이
   **40mV 미만이면 저항 재튜닝**(태윤 보유: 10/100/150/220/330/470/510/1k/2k/4.7k/10k/100k/1M/10M).
3. **[추측] 판정표 잔여 편의.** 현 판정표는 구회로 ADC 특성(+30mV, +3.3%)의 외삽이다.
   신회로 실측이 창을 벗어나면 실측 중심으로 재고정해야 한다(창 산출 로직: 관측범위+여유,
   조합 금지점과 55~70% 마진 — 대화에서 파이썬으로 수행했으며 재현 가능).
4. **[미확인] Ring 100Ω / Tip 220Ω 실물 장착 여부.** 문서(NETLIST/ASSEMBLY)에는 반영됐으나
   태윤이 브레드보드에 실제로 넣었는지 대화에서 확인되지 않았다. 미장착 상태에서 TS 플러그를
   꽂으면 3V3 레일 단락. **태윤에게 직접 확인할 것.**
5. **[사실] tuner.c HOP 384는 sim 공유.** sim 튜너 반응 속도도 절반이 되었다. 문제 시
   ESP/sim 분기 상수가 아니라 다른 해법(analyze 태스크 분리)을 논의할 것 — 파일 분기는
   "동일 DSP 공유" 원칙을 깬다.
6. **[사실] 오디오 입력 미연결.** 튜너/온셋의 실기 검증은 전부 이 연결 이후에만 유효하다.
   지금 "튜너가 안 잡힌다"는 보고가 오면 하드웨어 미연결이 1순위 원인이다.
7. **[구조 리스크] CPU0 과밀.** display(4)+input(3 예정)+LVGL이 CPU0에 몰려 있다. 앞으로
   CPU0에 태스크·부하를 추가하는 모든 변경은 워치독 재발 위험을 계산 후 진행할 것.
