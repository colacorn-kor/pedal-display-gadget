# HARDWARE_LIB_NOTES.md — KiCad 라이브러리 (SSOT 기준 재작성본)

> `NETLIST_SPEC.md`(SSOT)의 네트 끝점 이름에 심볼 핀 이름을 맞춰 생성.
> 목적: 스키매틱 → `.net` export → NETLIST_SPEC diff가 깨끗하게 떨어지게.
> 치수(풋프린트)는 여전히 **DRAFT placeholder** — 발주 전 실측 필요.

## 1. 커스텀 심볼 (`pedal-display-gadget.kicad_sym`)
| 심볼 | SPEC 레퍼런스 | 핀 이름(= SPEC 끝점) | 풋프린트 |
|---|---|---|---|
| ESP32-S3-DevKitC-1 | U1 | G1..G47 + 5V/3V3/GND | 커스텀 2×22 |
| ST7796_Display_1x9 | U2 | GND VCC SCL SDA RST DC CS BL **SDA-0(NC)** | 스톡 `PinSocket_1x09` |
| PCM1808_Module_2x6 | U3 | SCKI BCK LRC OUT FMT MD1 MD0 VINL VINR +5V 3.3 GND | 스톡 `PinSocket_2x06` |
| TL072_DIP8 | U4 | **1=OUTA 2=-INA 3=+INA 4=V- 5=+INB 6=-INB 7=OUTB 8=V+** | 스톡 `DIP-8_W7.62mm` |
| MP1584_Buck | U5 | IN+ IN- OUT+ OUT- | 커스텀 |
| D_1N5819 | D1 | 1=anode 2=cathode | 스톡 `D_DO-41...` |
| Jack_6.35mm_Mono | J1,J2 | tip sleeve switch | 커스텀 |
| microSD_Adapter_1x6 | J3 | CS SCK MOSI MISO VCC GND | 스톡 `PinSocket_1x06` |
| PWR_9V_ELB040202 | PWR1 | 1=+ 2=- | 커스텀 |

**핀 전기타입(ERC):** GPIO=bidirectional, 5V=power_in, 3V3=power_out(DevKit이 출력),
GND=power_in, TL072 V+/V-=power_in, ADC OUT/MISO=output, 아날로그입력=passive, NC=no_connect.

**DevKit 핀번호 주의:** 1–22 좌열↓, 23–44 우열↓ 의 **임의 순번**(심볼↔풋프린트만 일치).
실물 Espressif 실크 번호와 다름 → 발주 전 대조 필수. 핀 **이름**(G4 등)은 SPEC과 일치하므로
네트 배선·diff에는 문제 없음.

## 2. 커스텀 풋프린트 (`.pretty/`, 전부 DRAFT 치수)
| 풋프린트 | 실측 TODO |
|---|---|
| ESP32-S3-DevKitC-1 (2×22, 행간 22.86) | 실제 행간·보드폭 |
| Jack_6.35mm_Mono_Panel (3패드) | 실물 잭 기구도면 |
| MP1584_Module (1×4) | 실물 패드 피치 |
| PWR_ELB040202 (2패드) | 실제 9V 커넥터 기구 |

## 3. 스톡 라이브러리 사용(별도 제작 안 함)
- 버튼 SW1–7: `Switch:SW_Push` (SPEC: SW#.1=GPIO, SW#.2=GND). SW7만 오프보드 스톰프 → 보드엔 `PinHeader_1x02`.
- 저항: `Device:R` — SPEC 값 레퍼런스 그대로 사용: **R100, R10k_a, R10k_b, R10k_rg, R15k, R1M**.
- 커패시터: `Device:C`(필름/세라믹) / `Device:CP`(전해) — **C1u_in, C1u_out, C100u_op, C100n_op, C100u_ref, C100n_ref**.
- 전원 LED: `Device:LED` + `Device:R`(330Ω) — SPEC 네트엔 없으니 추가 시 표시등용으로.

## 4. 스키매틱 배선 시 SPEC 핵심 체크 (재확인)
- **+5V ↔ +3V3 절대 미연결.** 부하는 전부 `+9V_PROT`(D1 뒤)에서만.
- **D1 방향:** anode=+9V_RAW, cathode(띠)=+9V_PROT.
- **Rg(R10k_rg) 반대끝 = VREF(4.5V)**, GND 아님 (단일전원 포화 방지). 게인 = 1+R15k/R10k_rg = 2.5×.
- **TL072 버퍼:** U4.6 = U4.7 (같은 네트 VREF). 분압 10k+10k → 4.5V.
- **커플링 캡 ≥1µF**(C1u_in/out, 저역 보존).
- **LCD/SD SCLK·MOSI 버스 공유**(LCD_SCLK=G12, LCD_MOSI=G13), CS만 분리(LCD=G2, SD=G47).
- **MUTE(G3):** 드라이버 회로 확정 전까지 **미배선**. 스키매틱엔 "TODO: mute gate driver" 블록만.
- **금지핀:** 0·45·46(스트래핑)·19·20(USB)·26~37(플래시/PSRAM)·38·48(RGB LED) 회피. G3만 스트래핑 예외.

## 5. 설치
`sym-lib-table`/`fp-lib-table`는 **프로젝트 루트**(.kicad_pro 옆)에 병합하거나,
Manage Libraries → Project 탭에서 `hardware/pedal-display-gadget.kicad_sym` /
`.pretty` 추가. 닉네임 `pedal-display-gadget` 통일.

## 6. 다음 단계 (최종 가젯 향해)
1. **스키매틱 그리기** — 위 심볼 배치 → NETLIST_SPEC대로 배선.
2. **넷리스트 export → SPEC diff** (SSOT 최초 검증).
3. **풋프린트 할당 + 실측 반영**(커스텀 4종 치수 확정).
4. **PCB 배치** — Edge.Cuts로 보드 외곽 + 화면/오디오IN·OUT/9V 잭 물리 위치 확정 ← 태윤 최종 목표의 핵심 단계.
5. **케이스** 설계로 연결.
