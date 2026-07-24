# NETLIST_SPEC — 스키매틱 기준 연결 스펙 (SSOT)

KiCad로 그릴 때 이 네트들을 그대로 구현하고, `File → Export → Netlist…`로 뽑은 `.net`을
이 문서와 대조(diff)한다. 형식: `NET_NAME: 끝점1, 끝점2, …` (부품.핀 표기).

> 참조 부품(레퍼런스 예시): `U1`=ESP32-S3-DevKitC-1, `U2`=ST7796 디스플레이,
> `U3`=PCM1808 모듈, `U4`=TL072, `U5`=MP1584, `D1`=1N5819, `J1`=입력잭, `J2`=출력잭,
> `J3`=SD 어댑터, `SW1..SW7`=버튼, `PWR1`=ELB040202(9V 입력).
> 저항/캡은 값으로 부른다(R10k_1 등). TL072 핀번호는 8핀 DIP 기준(1=OUTA,2=-INA,3=+INA,
> 4=V−,5=+INB,6=−INB,7=OUTB,8=V+).

---

## 1. 전원 네트

```
+9V_RAW    : PWR1.+, D1.anode
+9V_PROT   : D1.cathode(띠), U5(MP1584).IN+, R100.1        ← 역전압 보호 뒤. 부하는 전부 여기서
GND        : (모든 GND 공통 — PWR1.-, U5.IN-/OUT-, U1.GND, U2.GND, U3.GND, U4.4(V−),
             D-잭 슬리브, 각 디커플링 캡의 GND쪽 …)  ★ 스타 그라운드 한 점
+5V        : U5(MP1584).OUT+, U1(DevKit).5V, U3(PCM1808).+5V
+3V3       : U1(DevKit).3V3, U3(PCM1808).3.3, U2(ST7796).VCC
```

op-amp 전용 깨끗한 9V (RC 필터):
```
+9V_OPAMP  : R100.2, U4.8(V+), C100u_op.+, C100n_op.1
             (R100 = 100Ω 직렬,  C100u_op = 100µF 전해,  C100n_op = 100nF 세라믹, 둘 다 →GND)
```

검토 규칙: `+5V`와 `+3V3`는 **절대 서로 연결 금지**. `+9V_PROT` 이후에만 부하. 다이오드
방향은 `D1.anode=+9V_RAW / D1.cathode=+9V_PROT` (띠가 부하 쪽).

---

## 2. 가상 그라운드 4.5V (TL072 B쪽 버퍼)

```
VREF_DIV   : R10k_a.2, R10k_b.1, C100u_ref.+, C100n_ref.1, U4.5(+INB)
             (R10k_a: +9V_OPAMP↔VREF_DIV,  R10k_b: VREF_DIV↔GND → 분압 4.5V)
             (C100u_ref, C100n_ref → GND)
VREF       : U4.7(OUTB), U4.6(−INB), R1M.2, R10k_rg.2   ← 버퍼된 저임피던스 4.5V
```

검토 규칙: `U4.6`와 `U4.7`이 같은 네트(VREF)여야 팔로워(버퍼)가 성립. 분압 중점이
`+9V_OPAMP`와 `GND` 사이 10k+10k인지 확인.

---

## 3. 신호 경로 (패스스루 + 버퍼/게인)

```
GTR_IN     : J1.tip(입력잭), J2.tip(출력잭), C1u_in.1     ← 통과(패스스루) + 탭 분기
AIN_P      : C1u_in.2, U4.3(+INA), R1M.1                  ← +입력 (1MΩ로 VREF 바이어스)
AIN_N      : U4.2(−INA), R15k.1, R10k_rg.1                ← −입력 (Rf/Rg 접합)
            (R15k: AIN_N↔U4.1(OUTA) = Rf 피드백)
            (R10k_rg: AIN_N↔VREF = ★Rg를 GND 아님 VREF에! 단일전원 핵심)
PCM_IN     : U4.1(OUTA)…아니라 C1u_out 경유 → 아래
(경유)      : U4.1(OUTA), C1u_out.1
PCM_INL    : C1u_out.2, U3(PCM1808).VINL                  ← 출력 커플링 1µF
```

검토 규칙: 게인 = 1 + R15k/R10k_rg = 2.5×. `R10k_rg`의 반대끝이 **VREF(4.5V)** 여야 함
(GND면 출력 포화). 입력 커플링 `C1u_in`은 1µF 이상(30Hz 보존).

---

## 4. 디스플레이 (SPI) — U1 ↔ U2

```
LCD_SCLK   : U1.G12, U2.SCL      (+ SD와 공유, 6절 참조)
LCD_MOSI   : U1.G13, U2.SDA      (+ SD와 공유)
LCD_CS     : U1.G2,  U2.CS
LCD_DC     : U1.G21, U2.DC
LCD_RST    : U1.G14, U2.RST
LCD_BL     : U1.G1,  U2.BL
(NC)       : U2.SDA-0            ← 연결 안 함 (MISO 미사용)
```

---

## 5. I2S 오디오 — U1 ↔ U3

```
I2S_MCLK   : U1.G8,  U3.SCKI
I2S_BCK    : U1.G9,  U3.BCK
I2S_WS     : U1.G18, U3.LRC
I2S_DIN    : U1.G10, U3.OUT(DOUT)
```

검토 규칙: U3 모드 점퍼 = 슬레이브/ I2S(스키매틱 주석으로 표기). U3 `+5V`/`3.3` 분리 확인.

---

## 6. SD 어댑터 (물리 배선만, 펌웨어 스텁) — U1 ↔ J3

```
LCD_SCLK   : (+ J3.SCK)          ← 4절 SCLK 네트에 J3.SCK 추가(버스 공유)
LCD_MOSI   : (+ J3.MOSI)         ← 4절 MOSI 네트에 J3.MOSI 추가
SD_MISO    : U1.G11, J3.MISO     ← SD 전용
SD_CS      : U1.G47, J3.CS       ← SD 전용
             J3.VCC → +5V 또는 +3V3(모듈 사양),  J3.GND → GND
```

> 즉 `LCD_SCLK`·`LCD_MOSI`는 U2와 J3가 **함께 매달린 한 네트**. CS만 각자(LCD=G2, SD=G47).

---

## 7. TRS 6키 저항 래더 / 풋스위치

```
TRS_LADDER_ADC : U1.G4 (ADC1_CH3), TRS_MAIN.Tip
TRS_MAIN.Ring  : +3V3
TRS_MAIN.Sleeve: GND

Ring(+3V3) ─ Rtop 10k ─ Tip
Tip ─ 각 키 ─ Sleeve(GND):
  UP=0Ω, DOWN=470Ω, LEFT=1kΩ, RIGHT=2kΩ, OK=4.7kΩ, HOME=10kΩ

GPIO_RESERVED  : U1.G5, U1.G6, U1.G7, U1.G15, U1.G16
                 (미사용, Phase 2 예비)
FOOTSW          : U1.G17, SW7.1 (SW7.2 → GND)
```

검토 규칙: 기존 본체 GPIO 버튼 6개는 제거한다. G4는 디지털 입력이 아닌 ADC 입력이며
Rtop 10k가 외부 풀업을 겸한다. G5/G6/G7/G15/G16은 연결하지 않는다. FOOTSW만 내부
풀업을 쓰는 active-low GPIO 입력이다.

---

## 8. 뮤트 (J201) — ⚠ 회로 미확정, 지금은 배선 금지

```
MUTE_CTL   : U1.G3 → (게이트 드라이브 회로, 설계 예정) → J201.gate
```

> J201은 공핍형이라 게이트에 **음전압**이 필요 → ESP32(0~3.3V)로 직접 못 끔. `G3→게이트`
> 직결하지 말 것. 음전압 생성 + RC 소프트램프 회로를 별도 확정한 뒤 이 네트를 채운다.
> 그 전까진 스키매틱에 "TODO: mute gate driver" 블록으로만 표시.

---

## 9. 핀 사용 요약 (중복·금지핀 검토용)

사용: 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,21,47
- 금지핀 회피: 0·45·46(스트래핑, 비움), 19·20(USB), 26~37(옥타 플래시/PSRAM), 38·48(RGB LED).
- 스트래핑 3만 예외 사용(뮤트 출력). 12는 S3에선 일반 IO(구형 ESP32와 다름).
- 각 GPIO는 **정확히 하나의 기능**에만. (SD의 11·47은 물리 배선만.)

---

## 리뷰 시 내가 확인하는 것 (체크리스트)
- [ ] 각 네트 끝점이 위 스펙과 일치(핀 번호·부품)
- [ ] `+5V` ↔ `+3V3` 미연결, 전원 단일 소스
- [ ] `D1` 방향(anode=RAW / cathode=PROT)
- [ ] `R10k_rg` 반대끝 = VREF(4.5V)  ← 포화 방지
- [ ] `U4.6=U4.7`(버퍼), 분압 4.5V
- [ ] 커플링 캡 ≥1µF(저역 보존)
- [ ] LCD/SD SCLK·MOSI 버스 공유, CS 분리
- [ ] GPIO 중복 없음 / 금지핀 없음
- [ ] MUTE(G3)는 드라이버 회로 확정 전까지 미배선
