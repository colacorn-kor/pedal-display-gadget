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
[완료] 디스플레이 · 앱플랫폼 · 입력레이어 · 슬롯/NVS · 런처/테마 · music_events
       · S1 플랫폼 추상화/PC 시뮬레이터 · 렌더러 최적화 · 저장소 위생
[완료] input 태스크 WDT 핫픽스: 310초 무재발 · 정상 UI · FOOTSW 전환 확인
[완료] 신회로 TRS 6키 ADC 실측 · HOME/FOOTSW 롱 동작 확인
[완료] 결정론적 PC 시뮬레이터 smoke CLI · 합성 시각화/튜너 DSP · NVS 격리
       · Windows 기본 출력 WASAPI 루프백 입력
[완료] SDSPI/FATFS 기반 · SD Gallery · PC SD 폴더 · music/game 공통 카탈로그
[완료] D1 플랫폼 capability · 공통 스테레오 transport/mixer · PC SDL 출력
[완료] D2 Music: 코드 생성 내장 로비 트랙 · WAV 탐색/스트리밍 재생 · 플레이어 UI
[완료] D3 Game: 4칸 타일 로비 · 빈 타일 OK=이름 없는 내장 GG Cat · HOME 로비 복귀
[완료] D4 Metronome: sample-clock click · 박자/subdivision UI · Game 효과음
[완료] D5 Gallery: 상태 UI · 자연 정렬 · 손상 검사 · 메타데이터 · 선택 보존 새로고침
[완료] D6 Game: MIT Peanut-GB DMG `.gb` 실행 · 검증 타일 · 2x 화면 · `.sav` · 공통 입력
[완료] D7 MIDI Monitor · 공통 MIDI 서비스 · Windows WinMM 입출력
[현재 SW] 오디오·미디어·출력·MIDI 장치 선택 UI와 PC 패키징
[병행 HW] SD 기능 확인 · 자동 듀얼레인지 · TLV320DAC3100/AUX/헤드폰 · MIDI · PhotoMOS 뮤트
[확장] S2 코덱 ESP 백엔드 → S3 WiFi/OTA → S4 MIDI UART/BLE 백엔드 → S5 스크립트 로더
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
  JPG/PNG/BMP/GIF/BIN 표시와 PC Music WAV/내장 로비 재생은 구현됐다. Gallery는
  자연 정렬, 형식·치수·크기 표시, 손상 파일 오류와 선택 보존 새로고침을 PC/SD 공통
  코드로 처리한다. GG Music은 S2 코덱 백엔드가 생길 때 같은 공통 transport에 연결한다.
- 사용자-facing 이름은 Game이다. 첫 외부 코어는 MIT 라이선스의
  [Peanut-GB](https://github.com/deltabeard/Peanut-GB)를 고정 커밋으로 포함한다. DMG `.gb`의
  헤더·체크섬·크기·카트리지 형식을 검사해 실제 실행 가능한 파일만 타일에 표시하고 save RAM은
  같은 폴더의 `.sav`에 저장한다. PC와 GG가 같은 코어를 쓰며 현재 외부 게임 오디오는 무음이다.
- NES 등 다른 Retro-Go급 기종은 코어별 코드 크기와 라이선스를 다시 검토한 뒤 같은 어댑터에
  추가한다. GPLv2 전체 Retro-Go 포팅과 앱 파티션 확장은 자동 전제가 아니다.
- Gallery·Music·Game은 SD가 없어도 각각 `GG` 월페이퍼·내장 8비트 로비 음악·내장
  점프 게임으로 기본 동작한다. Music과 Game은 동시에 실행하지 않는다.

### S1.6. PC 기준 제품 (현재)
- PC 시뮬레이터는 테스트 창이 아니라 앱·런처·설정을 먼저 완성하는 기준 구현이며,
  GG 없이도 사용할 수 있는 독립 데스크톱 앱으로 개발한다.
- 공통 앱은 PC에서 먼저 완성하고, ESP에서는 화면·입력·오디오·저장소 백엔드만 바꾼다.
- PC의 WASAPI loopback/캡처 장치는 PCM1808 분석 입력을, 로컬 `GG/` 폴더는 MicroSD를
  대신한다. SDL 출력은 미래 GG 코덱과 같은 공통 48kHz 스테레오 transport/mixer를
  사용하며 Music의 로비 트랙·WAV, Metronome click과 Game 효과음이 이 경로에서
  동작한다. Windows WinMM은 공통 MIDI 서비스와 MIDI Monitor의 입출력 백엔드다.
- 상세 기능 격차와 D0~D8 순서는 `PC_SIMULATOR_PRODUCT.md`가 권위다.

### S2. 오디오 출력 (3.5mm 스테레오 = 헤드폰/AUX 활성화)
- 공통 재생 transport·믹서와 PC SDL 출력, Music·Metronome UI와 앱 효과음은 구현됐다.
  현재 GG는 Metronome을 무음 시각 모드로 실행하며, Music과 실제 click/effect 출력은
  코덱 백엔드가 생길 때 같은 공통 경로에 연결한다.
- 재생 모듈은 **Adafruit TLV320DAC3100 Product 6309**로 확정했다. G40(DOUT),
  G41(SDA), G42(SCL), G7(RST)을 쓰고 PCM1808과 G9(BCK)·G18(WS)를 공유한다.
- 소프트웨어: `audio_playback` API(앱사운드 믹서, 미래 Core1 소비) + 기존 패스스루는
  아날로그 그대로.
- 앱 계약은 물리 `needs_codec` 대신 `required_capabilities`로 플랫폼 오디오 출력 능력을
  요구한다.
- **HW**: 코덱 모듈, AUX 커플링/감쇠와 헤드폰 잭 배선은 `ASSEMBLY.md`에 확정했다.
  TLV320DAC3100의 AIN1/AIN2 analog mix-in을 AUX L/R로 쓰며 별도 헤드폰 앰프는 없다.
- **SW**: ESP I2S0 full-duplex TX와 I2C 초기화, playback capability 연결은 아직 남았다.
- 앱 소리·음악·메트로놈은 헤드폰 경로에만 섞고, 하드와이어 기타 Thru에는 섞지 않는다.

### S3. 무선 (WiFi 우선, BLE 보조)
- ★사실: **ESP32-S3 = WiFi+BLE. BT Classic 없음 → 블루투스 오디오(A2DP) 불가.**
- **WiFi 웹 업로더** = 탈옥 플랫폼의 배포 경로: 폰/PC 브라우저 → 테마(.ggt)·이미지·(추후)앱
  업로드, SD보다 편함. + **OTA 펌웨어 업데이트**. AP모드(비번) 기본, 설정에서 on/off.
- **BLE**: BLE-MIDI(폰 앱 연동), 컴패니언 제어. HW 추가 불필요(칩 내장).

### S4. MIDI (공통 앱·PC 완료, GG 전송계층 대기)
- 공통 parser·메시지 이력·Program Change 상태·capture 모드와 MIDI Monitor 앱은 구현됐다.
  PC는 WinMM 입출력과 장치 열거를 제공한다.
- GG 플랫폼은 현재 명시적 unavailable stub이다. 물리 회로는 TRS-A 3.5mm 두 개,
  6N138 절연 IN(G5)과 SN74AHCT14 5V 표준 OUT(G6)으로 확정했다. UART와 BLE-MIDI를
  같은 `midi_feed()` 경계에 연결한다.
- **HW**: 프로토타입 배선과 구매표를 확정했다. **SW**: UART backend와 capability 연결이 남았다.

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
- SD 모듈 배선은 완료됐고 기능 확인이 남았다. 오디오 입력은 임시 TL072를 다시 만들지 않고
  OPA2192 자동 듀얼레인지 목표 회로로 진행한다.
- [x] 재생/AUX/헤드폰 모듈 선정: Adafruit TLV320DAC3100 Product 6309
- [x] MIDI IN/OUT과 PhotoMOS 뮤트 목표 배선 확정
- [ ] `hardware/PURCHASE_LIST.md` 부품 조달과 `ASSEMBLY.md` 배선 반영
- [x] PCM1808 두 채널 자동 듀얼레인지·Range Diagnostics·교정값 주입 경로
- [ ] OPA2192 부품 조달 뒤 목표 회로의 자동 범위 전환 실기 검증
- [ ] HOT/SENSITIVE 범위별 1kHz 및 20Hz~20kHz sweep 교정
- [x] `SZH-EKBZ-005` 배선
- [ ] `SZH-EKBZ-005` 공유 SPI 실기 브링업과 G39 PhotoMOS 뮤트 실기 확인
- [ ] KiCad Phase1 스키매틱(연습) → .net export → AI 검토 루프
- [ ] (PCB 리비전 시) 확정 MIDI/코덱/뮤트 회로, USB Host 전원 스위치·ESD, GG Analog Meter 반영
- [ ] (S6 착수 시) Smart 컨트롤러 MCU·LED 전력 예산과 Ring 검출 후 급전 회로 확정

## 5. 다음 세션 시작 절차
1) `git status`와 최근 커밋 확인 → 2) `LAB_STATE.md`의 마지막 플래시 확인 →
3) `PUNCHLIST.md`의 가장 높은 우선순위 선택 → 4) 관련 SSOT와 실코드 대조 →
5) 자동 검증과 필요한 실기 절차까지 포함해 완료. 추측 금지·확인 우선.
