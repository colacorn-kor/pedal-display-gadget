# HARDWARE_LIB_NOTES.md — KiCad 심볼/풋프린트 초안 (DRAFT)

> **성격:** KiCad 9.0.x에서 열어 감을 잡는 **가상 초안**. 배선·배치·치수는 이후 수정 전제.
> 최종 핀네임/네트는 `NETLIST_SPEC.md`(SSOT)와 대조해 재정렬해야 함(현재 미반영).

## 1. 포함 파일
```
hardware/
├─ pedal-display-gadget.kicad_sym        # 커스텀 심볼 5종
├─ pedal-display-gadget.pretty/          # 커스텀 풋프린트 3종
│   ├─ ESP32-S3-DevKitC-1.kicad_mod
│   ├─ Jack_6.35mm_Mono_Panel.kicad_mod
│   └─ MP1584_Module.kicad_mod
├─ sym-lib-table  / fp-lib-table          # 등록용 스니펫(설치법 §5)
└─ HARDWARE_LIB_NOTES.md
```

## 2. 커스텀 심볼 (모듈류 = 헤더소켓 장착)
| 심볼 | 핀수 | 풋프린트 매핑 | 비고 |
|---|---|---|---|
| ESP32-S3-DevKitC-1 | 44 (2×22) | 커스텀(동일 lib) | 사용 GPIO만 이름 지정, 나머지 `NC` |
| ST7796_Display_1x9 | 9 | `PinSocket_1x09_P2.54mm`(스톡) | SDA-0=MISO는 `NC` |
| PCM1808_Module_2x6 | 12 (2×6) | `PinSocket_2x06_P2.54mm`(스톡) | 아래 §6 전원변경 주의 |
| MP1584_Buck | 4 | 커스텀(동일 lib) | 5.0V 선세팅 |
| microSD_Adapter_1x6 | 6 | `PinSocket_1x06_P2.54mm`(스톡) | VCC 3V3? §6 미확정 |

**핀 번호 규약(초안 내부용):** DevKit은 1–22=좌열↓, 23–44=우열↓ 의 **임의 순번**.
실물 DevKitC-1 실크 번호와 다름 → 발주 전 Espressif 도면과 대조 필요.
심볼↔풋프린트 번호는 서로 1:1 일치하도록 생성되어 ERC/네트는 정상 동작.

**핀 전기타입(ERC용):** GPIO=bidirectional, 5V=power_in, 3V3=power_out,
GND=power_in, 미사용=no_connect. → netlist diff/ERC가 실수 잡도록 명시.

## 3. 커스텀 풋프린트 (전부 DRAFT 치수)
| 풋프린트 | 치수 근거 | 실측 TODO |
|---|---|---|
| ESP32-S3-DevKitC-1 | 2×22 / 2.54피치 / **행간격 22.86mm 가정** | 실제 행간·보드폭 |
| Jack_6.35mm_Mono_Panel | 3패드(T/S/N) 러프 | 실물 잭 기구도면 |
| MP1584_Module | 1×4 러프 | 실물 패드 피치/배치 |

UUID는 넣지 않음 → KiCad가 로드 시 자동 부여.

## 4. 스톡 라이브러리로 처리(별도 제작 안 함)
| 부품 | 심볼 | 풋프린트 |
|---|---|---|
| U4 TL072 | `Amplifier_Operational:TL072` | `Package_DIP:DIP-8_W7.62mm`(소켓 권장) |
| D1 1N5819 | `Device:D` | `Diode_THT:D_DO-41_...` (띠=부하쪽) |
| SW1–6 택트 | `Switch:SW_Push` | `Button_Switch_THT:SW_PUSH_6mm` |
| SW7 풋스위치 | 오프보드 → 보드엔 커넥터만 | 스톡 `PinHeader_1x02` (스톰프 스위치는 패널장착·배선) |
| R / C / LED | `Device:R` `Device:C` `Device:LED` | THT (`R_Axial_...`, `C_Disc_...`, `CP_Radial_...`, `LED_D5.0mm`) |

## 5. 라이브러리 설치
`sym-lib-table` / `fp-lib-table`는 **프로젝트 루트**(.kicad_pro 옆)에 두는 파일.
- 방법 A: 위 스니펫 내용을 프로젝트 루트 동명 파일에 병합(`${KIPRJMOD}` = 프로젝트 폴더).
- 방법 B: KiCad에서 Preferences → Manage Symbol/Footprint Libraries → Project 탭 →
  각각 `hardware/pedal-display-gadget.kicad_sym`, `hardware/pedal-display-gadget.pretty` 추가.
닉네임은 `pedal-display-gadget`으로 통일(심볼의 Footprint 속성이 이 닉네임 참조).

## 6. 심볼에 박아둔 설계 리마인더 / 변경점
- **PCM1808 전원 변경:** 모듈에 **+5V 공급 + 온보드 3.3 생성**. → `3.3` 핀은 레귤레이터
  출력이므로 **연결하지 않음**(심볼상 power_out/NC 표기). 기존 "DevKit 3V3→PCM1808 3.3"
  결정은 **폐기**. VINL/VINR=passive(아날로그 입력).
- **뮤트 G3:** J201 음전압 게이트 드라이버 미확정 → 심볼/스키매틱은 `G3/MUTE` 네트까지만.
  드라이버 서브회로 미작성(Phase 2 파킹). J201 게이트 직결 금지 원칙 유지.
- **op-amp Rg→VREF:** TL072 스톡 심볼 사용. 스키매틱 배선 시 게인저항 Rg는
  **GND 아니라 VREF(4.5V)** 로 (단일전원 포화 방지). 심볼엔 강제 불가 → 배선 주의.
- **SPI 버스 공유:** LCD/SD SCK=G12·MOSI=G13 공유, CS만 개별(LCD=G2, SD=G47).
- **SD VCC 미확정:** `VCC(3V3?)`로 표기. 3.3 직결 vs 5V+온보드LDO 확인 후 확정.

## 7. 다음 단계 TODO (최종본 전)
1. `NETLIST_SPEC.md` 붙여넣기 → 심볼 핀네임/네트명 SPEC에 맞춰 재정렬.
2. 실측 3종(DevKit 행간·6.35잭·MP1584) 반영해 풋프린트 치수 확정.
3. DevKit 심볼 핀번호를 실물 실크와 일치시킬지 결정(현재 임의 순번).
4. 스키매틱 그림 → `.net` export → NETLIST_SPEC와 diff (기존 리뷰 루프).
