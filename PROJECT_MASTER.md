# PROJECT_MASTER.md — GG 총괄 로드맵 · 워크플로우 · 확장 아키텍처

> 이 문서는 **프로젝트 전체의 SSOT 지도**다.
> 원칙: 태윤=제품 방향·물리 조작 / Codex=설계·구현·자동 검증·리뷰·로그 분석. 대화는 한국어.

## 0. 문서 지도 (SSOT 맵)
| 문서 | 관할 |
|---|---|
| `AGENTS.md` | 에이전트의 지속 작업 규칙·검증·하드웨어 안전 |
| `LAB_STATE.md` | 마지막 플래시·현재 장치 상태·다음 실기 절차 |
| `PUNCHLIST.md` | 미결 작업과 우선순위 |
| `GG_PRODUCT_SPEC.md` | GG 제품 정체성·범위·GG2 경계·외부 확장 |
| `ARCHITECTURE.md` | 펌웨어 인앱 구조(코어분리·앱모델·입력규약) |
| `hardware/NETLIST_SPEC.md` | 회로 넷 연결(KiCad 대조 기준) |
| `hardware/AS_BUILT_WIRING.md` | 태윤이 관리하는 현재 실물 브레드보드 배선 |
| `ASSEMBLY.md` | Codex가 제안하는 다음 적용 배선 |
| `hardware/AUDIO_FRONTEND_ENGINEERING.md` | 오디오 목표 회로의 조달 조건·측정·교정 메모 |
| `CONTROLLER_DESIGN.md` | Basic 저항 래더·Smart MCU 6키 컨트롤러 계약 |
| `LAUNCHER_DESIGN.md` | 런처·슬롯·영속성·열린플랫폼 |
| `UI_DESIGN.md` | 디자인시스템·테마토큰·.ggt 포맷 |
| `PC_SIMULATOR_PRODUCT.md` | PC 기준 제품·플랫폼 치환·ESP 이식 순서 |
| `CLAUDE_HANDOFF.md` | 2026-07-26 인수인계 스냅샷(현재 상태의 SSOT 아님) |
| **본 문서** | 로드맵·워크플로우·확장 트랙 |

## 1. 통합 작업 흐름
1) 태윤과 Codex가 요구사항 확정 → 2) 공통 앱/UI를 PC 시뮬레이터에서 구현·검증 →
3) ESP 플랫폼 백엔드를 연결하고 전체 빌드 → 4) 목적별 커밋 → 5) 필요한 경우에만 USB
플래시·로그 수집 → 6) 태윤의 화면·버튼·소리 관찰과 로그를 판정 → 7) SSOT 갱신.
- 복잡하거나 위험한 변경만 `CODEX_INSTRUCTION_*.md`로 실행 계약을 남긴다.
- 일회용 지시서는 적용·검증 후 삭제하고 지속 규칙은 `AGENTS.md`에 반영한다.
- 하드웨어 없이 끝낼 수 없는 항목은 자동 검증 통과와 실기 검증 대기를 구분해서 보고한다.
- 매 단계에서 dirty 상태, 파생 `sdkconfig`, 누락된 하드웨어 파일, 문서 stale을 확인한다.

## 2. 로드맵 (현재 위치 → 확장)
```
[완료] 디스플레이 · 앱플랫폼 · 입력레이어 · 슬롯/NVS · 런처/테마 · music_events/Bounce
       · S1 플랫폼 추상화/PC 시뮬레이터 · 렌더러 최적화 · 저장소 위생
[완료] input 태스크 WDT 핫픽스: 310초 무재발 · 정상 UI · FOOTSW 전환 확인
[완료] 신회로 TRS 6키 ADC 실측 · HOME/FOOTSW 롱 동작 확인
[완료] 결정론적 PC 시뮬레이터 smoke CLI · 합성 시각화/튜너 DSP · NVS 격리
       · Windows 기본 출력 WASAPI 루프백 입력
[완료] SDSPI/FATFS 기반 · SD Gallery · PC SD 폴더 · music/game 공통 카탈로그
[현재 SW] Sound Monitor 우선 최적화 → PC 기준 제품 D0/D1
[다음 SW] 플랫폼 능력·PC 오디오 출력 → Music → Gallery UX → Game
[병행 HW] SD 기능 확인 · TL072 분석 탭 재배선 · 자동 듀얼레인지 · KiCad 목표 회로
[확장] S2 코덱 출력 → S3 WiFi/OTA → S4 MIDI(UART+BLE) → S5 스크립트 로더
       → S6 스마트 컨트롤러 → S7 GG Analog Meter
```

## 3. 확장 트랙 아키텍처 (밑바탕 확정)
### S1. 플랫폼 추상화 (완료)
- UI·앱 로직(screen_manager/apps/renderers/theme/launcher)과 ESP 하드웨어 코드
  (display_bringup/오디오태스크/GPIO/NVS)를 `platform_*` 인터페이스로 분리.
- 효과: PC 시뮬레이터에서 같은 앱·UI·renderer·`fft_map`이 SDL 창으로 실행된다.
  입력 수집과 FFT 실행만 플랫폼 백엔드로 분리돼 테스트가 쉽고, 코덱/무선 추가 시 UI는
  바뀌지 않는다.

### S1.5. SD 콘텐츠 플랫폼 (기반·Gallery 완료)
- LCD와 SPI2의 G12/G13을 공유하고 SD 전용 G11 MISO·G47 CS를 쓴다. Gallery가
  처음 필요할 때 10MHz SDSPI/FATFS를 마운트하므로 카드가 없어도 기본 부팅은 유지된다.
- 실제 모듈은 `SZH-EKBZ-005`이며 VCC는 `+5V`(4.5~5.5V), SPI 신호는 온보드
  레벨 변환을 거치는 3.3V 로직이다.
- `/GG/images`, `/GG/music`, `/GG/games`를 공통 카탈로그로 읽는다. Gallery의
  JPG/PNG/BMP/GIF/BIN 표시는 구현됐다. Music 재생은 PC 출력에서 먼저 구현하고 GG에서는
  S2 코덱 백엔드에 연결한다.
- 사용자-facing 이름은 Game이다. 외부 게임 실행은 공식
  [Retro-Go](https://github.com/ducalex/retro-go) 코어의 포팅·GPLv2
  라이선스 결정·확장 파티션 승인 뒤 진행한다.
  현재는 ROM 파일 판별과 정렬 기반만 있으며 게임 실행을 가장하지 않는다.
- Gallery·Music·Game은 SD가 없어도 각각 `GG` 월페이퍼·내장 8비트 로비 음악·내장
  점프 게임으로 기본 동작한다. Music과 Game은 동시에 실행하지 않는다.

### S1.6. PC 기준 제품 (현재)
- PC 시뮬레이터는 테스트 창이 아니라 앱·런처·설정을 먼저 완성하는 기준 구현이며,
  GG 없이도 사용할 수 있는 독립 데스크톱 앱으로 개발한다.
- 공통 앱은 PC에서 먼저 완성하고, ESP에서는 화면·입력·오디오·저장소 백엔드만 바꾼다.
- PC의 WASAPI loopback/캡처 장치는 PCM1808 분석 입력을, 로컬 `GG/` 폴더는 MicroSD를
  대신한다. 다음 기반은 PC 오디오 출력이며 미래 GG 코덱과 같은 재생 API를 사용한다.
- 상세 기능 격차와 D0~D6 순서는 `PC_SIMULATOR_PRODUCT.md`가 권위다.

### S2. 오디오 출력 (3.5mm 스테레오 = 헤드폰/AUX 활성화)
- 공통 재생 transport·믹서와 Music/Metronome UI는 PC 오디오 출력에서 먼저 완성한다.
- 코덱(I2S TX) 추가: **예약핀 G40(DOUT)·G41(SDA)·G42(SCL)** 사용(권장 배선표에서 비움).
- 소프트웨어: `audio_out` API(앱사운드 믹서, Core1 생산) + 기존 패스스루는 아날로그 그대로.
- 앱 계약 확장: `needs_codec` 임시 판정을 플랫폼 오디오 출력 능력으로 일반화한다.
- **HW(태윤)**: 코덱모듈 선정 필요 — 출력만이면 PCM5102A(무I2C·간단), 입출력 통합이면 ES8388.
  선정되면 NETLIST_SPEC 확장 → 브레드보드.
- 앱 소리·음악·메트로놈은 헤드폰 경로에만 섞고, 하드와이어 기타 Thru에는 섞지 않는다.

### S3. 무선 (WiFi 우선, BLE 보조)
- ★사실: **ESP32-S3 = WiFi+BLE. BT Classic 없음 → 블루투스 오디오(A2DP) 불가.**
- **WiFi 웹 업로더** = 탈옥 플랫폼의 배포 경로: 폰/PC 브라우저 → 테마(.ggt)·이미지·(추후)앱
  업로드, SD보다 편함. + **OTA 펌웨어 업데이트**. AP모드(비번) 기본, 설정에서 on/off.
- **BLE**: BLE-MIDI(폰 앱 연동), 컴패니언 제어. HW 추가 불필요(칩 내장).

### S4. MIDI (Phase 2 예정대로)
- 물리: TRS-A 3.5mm + 옵토커플러(IN)·전류원(OUT), UART. 파서(midi.c)·매핑 이미 존재.
- BLE-MIDI를 같은 midi.c 이벤트로 합류(전송계층만 다름).
- **HW(태윤)**: 옵토(6N138류)·TRS-A잭 2개, 프로토타입 제외 결정 유지 → PCB 리비전에서.

### S5. 스크립트 앱 로더 (열린 플랫폼 완성)
- LAUNCHER_DESIGN §6 확장점 준수됨(동적등록·id키잉) → 인터프리터(Lua류)가
  `gadget_app_t` thunk로 등록. 배포는 S3 웹업로더/SD. 가장 마지막.

### S6. 스마트 컨트롤러 (MCU 6키)
- 현재 Basic 저항 래더와 같은 TRS 잭·G4 `TRS_SIG`·입력 이벤트 계층을 재사용한다.
- MCU 기반 6키 스캔과 오픈드레인 반이중 통신, 테마 연동 LED를 추가한다.
- 전기 계약과 자동 판별, 보호 회로의 SSOT는 `CONTROLLER_DESIGN.md`다.

### S7. GG Analog Meter
- 외부 USB 장치 1종은 범용 화면이 아니라 움직이는 바늘의 GG 보조 미터로 한정한다.
- GG가 교정한 입력 Vrms·dBu·dBV·제품 기준 dBFS·VU/peak를 USB Host에서
  vendor-defined HID로 20~30Hz 전송한다.
- 미터는 로컬 DAC/PWM과 바늘 드라이버를 가지며 오디오 신호와 Thru에는 연결하지 않는다.

## 4. 하드웨어 태윤 TODO (소프트웨어와 비동기)
- 현재 실물은 `hardware/AS_BUILT_WIRING.md`, 다음 권장 연결은 `ASSEMBLY.md`가 맡는다.
- Fritzing 보조 도면은 `hardware/as-built/GG_PROTOTYPE.fzz`에 둔다.
- SD 모듈 배선은 완료됐고 기능 확인이 남았다. 오디오 입력 구간은 TL072 기준 재배선이 필요하다.
- [ ] 코덱 모듈 선정(PCM5102A vs ES8388) — S2 착수 조건
- [ ] 현재 TL072 LINE/INST 프로토타입 재배선과 외부 9V 기준 측정
- [x] PCM1808 두 채널 자동 듀얼레인지·Range Diagnostics·교정값 주입 경로
- [ ] OPA2192 부품 조달 뒤 목표 회로의 자동 범위 전환 실기 검증
- [ ] HOT/SENSITIVE 범위별 1kHz 및 20Hz~20kHz sweep 교정
- [x] `SZH-EKBZ-005` 배선
- [ ] `SZH-EKBZ-005` 공유 SPI 실기 브링업과 뮤트 회로 확정
- [ ] KiCad Phase1 스키매틱(연습) → .net export → AI 검토 루프
- [ ] (PCB 리비전 시) MIDI, 코덱, USB Host 전원 스위치·ESD, GG Analog Meter 반영
- [ ] (S6 착수 시) Smart 컨트롤러 MCU·LED 전력 예산과 Ring 검출 후 급전 회로 확정

## 5. 다음 세션 시작 절차
1) `git status`와 최근 커밋 확인 → 2) `LAB_STATE.md`의 마지막 플래시 확인 →
3) `PUNCHLIST.md`의 가장 높은 우선순위 선택 → 4) 관련 SSOT와 실코드 대조 →
5) 자동 검증과 필요한 실기 절차까지 포함해 완료. 추측 금지·확인 우선.
