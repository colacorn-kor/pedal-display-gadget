# PROJECT_MASTER.md — GUI/GG 총괄 로드맵 · 워크플로우 · 확장 아키텍처

> 이 문서는 **프로젝트 전체의 SSOT 지도**. 이후 세션(Opus)과 Codex는 이 문서에서 시작한다.
> 원칙: 태윤=방향결정·하드웨어·조립 / AI(Opus)=설계·리뷰·지시서 / Codex=구현. 대화는 한국어.

## 0. 문서 지도 (SSOT 맵)
| 문서 | 관할 |
|---|---|
| `ARCHITECTURE.md` | 펌웨어 인앱 구조(코어분리·앱모델·입력규약) |
| `hardware/NETLIST_SPEC.md` | 회로 넷 연결(KiCad 대조 기준) |
| `ASSEMBLY.md` | 브레드보드 조립(게인스위치·TRS·코덱자리 포함) |
| `LAUNCHER_DESIGN.md` | 런처·슬롯·영속성·열린플랫폼 |
| `UI_DESIGN.md` | 디자인시스템·테마토큰·.ggt 포맷 |
| **본 문서** | 로드맵·워크플로우·확장 트랙 |

## 1. 검증된 워크플로우 (유지)
1) 태윤 결정 → 2) AI가 **실제 코드/문서 확인 후** 설계·Codex지시서 작성(추측 금지) →
3) Codex 구현·커밋 → 4) AI가 지시서의 불변조건·완료판정 대비 검토 → 5) 태윤 플래시·실기확인.
- 지시서 필수 요소: 목표범위(❌포함)·실제심볼 근거·불변조건·-Werror규칙·완료판정·산출물형식.
- 교훈: 매 단계 **커밋+푸시 확인**(-dirty 사고 방지), `.pretty`류 폴더 누락 주의, 문서 stale 즉시 수정.

## 2. 로드맵 (현재 위치 → 확장)
```
[완료] 디스플레이 브링업 · 입력② · 렌더러최적화(curve) · 슬롯+NVS③-A
[지시서 발행됨] ③-B/C 런처+테마 → music_events+Bounce   ← 다음 Codex 투입분
[다음] S1 플랫폼 추상화 + PC 시뮬레이터  ← CODEX_INSTRUCTION_pc_simulator.md
[그후] S2 코덱 오디오출력 → S3 WiFi 업로더/OTA → S4 MIDI(UART+BLE) → S5 스크립트 앱로더
병행: bars/reactive 부분갱신, TRS 6키 ADC 리워크(케이블 연결 시점)
```

## 3. 확장 트랙 아키텍처 (밑바탕 확정)
### S1. 플랫폼 추상화 (모든 확장의 전제)
- UI·앱 로직(screen_manager/apps/renderers/theme/launcher)과 ESP 하드웨어 코드
  (display_bringup/오디오태스크/GPIO/NVS)를 `platform_*` 인터페이스로 분리.
- 효과: PC 시뮬레이터(같은 C가 SDL 창에서 실행), 테스트 용이, 코덱/무선 추가 시 UI 무변경.

### S2. 오디오 출력 (3.5mm 스테레오 = 헤드폰/AUX 활성화)
- 코덱(I2S TX) 추가: **예약핀 G40(DOUT)·G41(SDA)·G42(SCL)** 사용(ASSEMBLY에 확보됨).
- 소프트웨어: `audio_out` API(앱사운드 믹서, Core1 생산) + 기존 패스스루는 아날로그 그대로.
- 앱 계약 확장: `needs_codec` 활성화(런처가 이미 비활성표시 지원 설계).
- **HW(태윤)**: 코덱모듈 선정 필요 — 출력만이면 PCM5102A(무I2C·간단), 입출력 통합이면 ES8388.
  선정되면 NETLIST_SPEC 확장 → 브레드보드.

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

## 4. 하드웨어 태윤 TODO (소프트웨어와 비동기)
- [ ] 코덱 모듈 선정(PCM5102A vs ES8388) — S2 착수 조건
- [ ] 3.5mm 스테레오 잭 2개(헤드폰/AUX)·TRS잭·게인스위치 등 ASSEMBLY대로 조립 계속
- [ ] KiCad Phase1 스키매틱(연습) → .net export → AI 검토 루프
- [ ] (PCB 리비전 시) MIDI 부품, 코덱 확정 반영

## 5. 다음 세션(Opus) 시작 절차
1) 본 문서 §2 로드맵에서 현재 위치 확인 → 2) 해당 지시서 존재 여부 확인(§0 지도) →
3) 없으면 **실코드 확인 후** 지시서 작성, 있으면 Codex 결과 검토부터. 추측 금지·확인 우선.
