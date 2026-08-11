# NETLIST_SPEC — 스키매틱 기준 연결 스펙 (SSOT)

KiCad로 그릴 때 이 네트들을 그대로 구현하고, `File → Export → Netlist…`로 뽑은 `.net`을
이 문서와 대조(diff)한다. 형식: `NET_NAME: 끝점1, 끝점2, …` (부품.핀 표기).

> **리비전 상태:** 아래 권위 넷은 GG 목표인 **무스위치 자동 듀얼레인지** 회로다. 현재
> 실물 브레드보드의 연결은 `hardware/AS_BUILT_WIRING.md`, 다음 권장 연결은
> `ASSEMBLY.md`, 장치 상태는 `LAB_STATE.md`에 기록한다.
> 2026-08-10 현재 사용자는 남은 하드웨어를 완성하기 위한 부품 조달과 배선 정리를
> 요청했다. 다음 권장 연결은 임시 TL072+SPDT가 아니라 아래 자동 듀얼레인지 목표 회로다.
> 조달표는 `hardware/PURCHASE_LIST.md`, 실장·측정·교정 조건은
> `hardware/AUDIO_FRONTEND_ENGINEERING.md`에 둔다. 부품 실장과 무전원 검사를 통과하기
> 전에는 듀얼 펌웨어를 플래시하지 않는다.
>
> 참조 부품(레퍼런스 예시): `U1`=ESP32-S3-DevKitC-1, `U2`=ST7796 디스플레이,
> `U3`=PCM1808 모듈, `U4/U6`=OPA2192 RRIO dual op-amp, `U5`=MP1584, `D1`=1N5822,
> `D2`=BAV199 저누설 dual-series clamp,
> `J1`=입력잭, `J2`=출력잭,
> `J3`=SZH-EKBZ-005 MicroSD 모듈, `J4`=AUX TRS, `J5`=MIDI IN TRS-A,
> `J6`=MIDI OUT TRS-A, `U7`=Adafruit TLV320DAC3100 모듈,
> `U8`=SN74AHCT14N, `U9`=6N138, `U10`=AQY221R2S PhotoMOS,
> `SW1..SW7`=버튼,
> `PWR1`=ELB040202(9V 입력), `D1`=1N5822 Schottky,
> `U11`=Adafruit LM66200 Ideal Dual Diodes Product 5830.
> 저항/캡은 값으로 부른다(R10k_1 등). OPA2192 핀번호는 표준 dual op-amp
> 8핀 기준(1=OUTA,2=-INA,3=+INA,4=V−,5=+INB,6=−INB,7=OUTB,8=V+).
> 현재 DIP TL072와 핀 기능은 같지만 최종 패키지와 footprint는 발주 부품에 맞춘다.

---

## 1. 전원 네트

```
+9V_RAW    : PWR1.+, D1.anode
+9V_PROT   : D1.cathode(띠), U5(MP1584).IN+, R100.1        ← 역전압 보호 뒤
GND        : (공통 GND — PWR1.-, U5.IN-/OUT-, U1.GND, U2.GND, U3.GND,
             U4.4(V−), U6.4(V−), D2.1(A1), J3.GND, U7.GND, U8.7, U9.5, U11.GND,
             J1/J2/J4 sleeve, J6.sleeve, 각 디커플링 캡의 GND쪽 …)
             ★ 스타 그라운드 한 점. J5 MIDI IN의 tip/ring/sleeve는 제외
BUCK_5V    : U5(MP1584).OUT+, U11.VIN1
+5V        : U11.VOUT, U1(DevKit).5V, U3(PCM1808).+5V, J3.VCC,
             U7.VIN, U8.14, U9.8
POWER_ISO_NC: U11.VIN2, U11.OFF, U11.STAT
+3V3       : U1(DevKit).3V3, U3(PCM1808).3.3, U2(ST7796).VCC,
             R_MIDI_RX_2K2.1
```

op-amp 전용 깨끗한 9V (RC 필터):
```
+9V_OPAMP  : R100.2, U4.8(V+), U6.8(V+), C100u_op.+,
             C100n_op.1, C100n_u6.1, D2.2(K2)
             (R100 = 100Ω 직렬,  C100u_op = 100µF 전해,  C100n_op = 100nF 세라믹, 둘 다 →GND)
```

검토 규칙: `+5V`와 `+3V3`는 **절대 서로 연결 금지**. D1은 9V 역극성을 막고 띠가 부하
쪽이다. U11은 USB에서 MP1584로의 역급전을 막는다. U11의 온보드 1MΩ `OFF` pull-down을
사용하므로 OFF는 비운다. U5 `OUT+`를 4.9~5.1V로 조정한다.

---

## 2. 가상 그라운드 (U4 B쪽 버퍼)

```
VREF_DIV   : R10k_a.2, R10k_b.1, C100u_ref.+, C100n_ref.1, U4.5(+INB)
             (R10k_a: +9V_OPAMP↔VREF_DIV,  R10k_b: VREF_DIV↔GND → 전원 레일의 절반)
             (C100u_ref, C100n_ref → GND)
VREF       : U4.7(OUTB), U4.6(−INB), R22M_sense.2,
             R10k_sense.2, R1M5_hot.2, C15p_hot.2, U6.5(+INB)
             ← 버퍼된 저임피던스 VREF. 9V 입력 시 보호 다이오드 강하를 포함해 보통 약 4.3V
U6_IDLE    : U6.7(OUTB), U6.6(−INB)
             (U6 B = VREF를 따르는 미사용 unity follower)
```

검토 규칙: `U4.6`와 `U4.7`이 같은 네트(VREF)여야 팔로워(버퍼)가 성립. 분압 중점이
`+9V_OPAMP`와 `GND` 사이 10k+10k인지 확인. 목표 U4/U6는 9V 단전원에서 HOT full-scale
공통모드와 출력 swing을 감당하는 OPA2192다. 현재 TL072를 목표 회로에 대입하지 않는다.

---

## 3. 신호 경로 (하드와이어 Thru + 자동 듀얼레인지)

```
GTR_IN       : J1.tip(입력잭), J2.tip(출력잭), C1u_in.1
               ← 전원·펌웨어와 무관한 직접 Thru + 분석 탭 분기
ANALYZER_TAP  : C1u_in.2, R100k_sense.1, R10M_hot.1, C3p3_hot.1
SENSE_P       : R100k_sense.2, U4.3(+INA), R22M_sense.1,
                D2.3(K1/A2)
               (R22M_sense: SENSE_P↔VREF)
               (D2 BAV199: GND(A1)→SENSE_P(K1/A2)→+9V_OPAMP(K2))
SENSE_N      : U4.2(−INA), R30k_sense.1, R10k_sense.1
               (R30k_sense: SENSE_N↔U4.1, R10k_sense: SENSE_N↔VREF)
SENSE_OUT    : U4.1(OUTA), R30k_sense.2, C1u_sense.1
               (op-amp gain 4.00×, 입력잭 기준 명목 약 3.98×)

HOT_DIV      : R10M_hot.2, R1M5_hot.1, C3p3_hot.2,
               C15p_hot.1, U6.3(+INA)
               (R10M_hot||C3p3_hot: ANALYZER_TAP↔HOT_DIV)
               (R1M5_hot||C15p_hot: HOT_DIV↔VREF)
HOT_OUT      : U6.1(OUTA), U6.2(−INA), C1u_hot.1
               (HOT gain = 1.5M/(10M+1.5M) ≈ 0.1304×)

PCM_SENSE_RC : C1u_sense.2, R100_sense.1
PCM_INR      : R100_sense.2, C10n_sense.1, U3(PCM1808).VINR
               (C10n_sense.2→GND)
PCM_HOT_RC   : C1u_hot.2, R100_hot.1
PCM_INL      : R100_hot.2, C10n_hot.1, U3(PCM1808).VINL
               (C10n_hot.2→GND)
```

검토 규칙:

- `J1.tip`과 `J2.tip`은 한 네트여야 하며 어떤 IC·스위치·직렬 부품도 사이에 넣지 않는다.
- SENSITIVE op-amp는 강한 입력에서 포화될 수 있다. `R100k_sense`와 저누설 D2가
  입력 전류를 제한해 op-amp·HOT·Thru를 격리한다. 저누설 부품을 쓰고 실제 기생
  커패시턴스를 sweep 교정에 포함한다.
- HOT은 9V op-amp로 버퍼하기 전에 수동 감쇠한다. OPA2192의 전형적 공통모드 입력
  커패시턴스 6.4pF를 포함한 첫 근사는 `10M×3.3pF≈33us`와
  `1.5M×(15pF+6.4pF)≈32.1us`다. 실제 저항·PCB 기생 커패시턴스를 포함한 값은
  C0G 병렬 튜닝 패드와 20Hz~20kHz sweep으로 조정한다.
- `HOT_DIV`, `SENSE_P`와 고값 저항은 솔더리스 브레드보드에 실장하지 않는다. 세척한
  만능기판 또는 PCB에서 입력 trace를 짧게 하고 전원·출력 trace와 떨어뜨린다.
- 분석 탭의 명목 DC 입력 임피던스는 `22.1M || 11.5M ≈ 7.56MΩ`이며 GG 목표 최소
  5MΩ를 넘는다.
- PCM1808 명목 3.0Vpp 기준 sine 입력 한계는 SENSITIVE 약 0.265Vrms,
  HOT 약 8.13Vrms다. 두 채널은 항상 동시에 샘플링하고 DSP가 overlap+hysteresis로 고른다.
- 입력·출력 커플링은 1µF 이상을 사용한다. 100Ω+10nF는 PCM1808 데이터시트의 선택형
  외부 RF 필터 범위 안에서 시작하는 값이며, 최종 주파수 응답은 sweep 교정으로 확정한다.
- `SW_GAIN`, `GAIN_LINE`, `GAIN_INST` 네트는 목표 회로에 존재하지 않는다.

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

## 5. I2S 오디오 — U1 ↔ PCM1808 U3 / 재생 DAC U7

```
I2S_MCLK   : U1.G8,  U3.SCKI
I2S_BCK    : U1.G9,  U3.BCK, U7.BCK
I2S_WS     : U1.G18, U3.LRC, U7.WSEL
I2S_ADC_DIN: U1.G10, U3.OUT(DOUT)
I2S_DAC_OUT: U1.G40, U7.DIN
```

검토 규칙: U3 모드 점퍼 = 슬레이브/I2S. U3 `+5V`/`3.3` 분리 확인. U7도 I2S
slave이며 `MCK`는 비우고 BCK를 내부 PLL 입력으로 사용한다. U1이 48kHz BCK/WS를 두
장치에 공통 공급하고 U3는 U1으로, U1은 U7으로 서로 다른 데이터선을 사용한다.

---

## 6. SZH-EKBZ-005 MicroSD 모듈 (현재 배선됨, 기능 확인 대기) — U1 ↔ J3

```
LCD_SCLK   : (+ J3.SCK)          ← 4절 SCLK 네트에 J3.SCK 추가(버스 공유)
LCD_MOSI   : (+ J3.MOSI)         ← 4절 MOSI 네트에 J3.MOSI 추가
SD_MISO    : U1.G11, J3.MISO     ← SD 전용
SD_CS      : U1.G47, J3.CS       ← SD 전용
             J3.VCC → +5V,  J3.GND → GND
```

> 즉 `LCD_SCLK`·`LCD_MOSI`는 U2와 J3가 **함께 매달린 한 네트**. CS만 각자(LCD=G2, SD=G47).
> `SZH-EKBZ-005`는 4.5~5.5V VCC와 온보드 3.3V LDO·레벨 변환을 쓰므로 `J3.VCC`를
> `+3V3`에 연결하지 않는다. `+5V`는 J3의 VCC에만 들어가며 SCK/MOSI/CS는 U1의 3.3V
> 출력, MISO는 J3의 3.3V 출력이다.

---

## 7. TRS 6키 저항 래더 / 풋스위치

```
TRS_SIG         : U1.G4 (Basic=ADC1_CH3, Smart=디지털 통신), R_TRS_TIP_220.2
TRS_MAIN.Tip    : R_TRS_TIP_220.1
TRS_MAIN.Ring   : R_TRS_RING_100.1
TRS_RING_3V3    : +3V3, R_TRS_RING_100.2
TRS_MAIN.Sleeve : GND

+3V3 ─ 100Ω ─ TRS_MAIN.Ring
TRS_MAIN.Tip ─ 220Ω ─ TRS_SIG ─ U1.G4

컨트롤러 내부:
Ring(+3V3) ─ Rtop 10k ─ Tip
Tip ─ 각 키 ─ Sleeve(GND):
  UP=0Ω, DOWN=470Ω, LEFT=1kΩ, RIGHT=2kΩ, OK=4.7kΩ, HOME=10kΩ

GPIO_RESERVED  : U1.G15, U1.G16
                 (미사용 예비)
FOOTSW          : U1.G17, SW7.1 (SW7.2 → GND)
```

검토 규칙: 기존 본체 GPIO 버튼 6개는 제거한다. G4의 `TRS_SIG`는 Basic에서 ADC 입력,
Smart에서 디지털 통신으로 재사용하며 과도한 RC 필터를 달지 않는다. Rtop 10k는 분압과
Smart 오픈드레인 풀업을 겸한다. Ring 전압은 **+3V3 고정이며 5V 연결 금지**다. Tip 직렬
220Ω은 G4를 보호하고, Ring 직렬 100Ω은 TS 플러그 삽입 시 Ring-Sleeve 단락 전류를
제한한다. G5/G6은 MIDI, G7은 코덱 reset에 사용하며 G15/G16만 비운다. 이 핀 계약은
`INPUT_TRS_LADDER=1` 제품 하드웨어 기준이다. FOOTSW만 내부 풀업을 쓰는 active-low
GPIO 입력이다.

---

## 8. TLV320DAC3100 재생 DAC, AUX, 헤드폰

```
CODEC_RST  : U1.G7, U7.RST
CODEC_SDA  : U1.G41, U7.SDA
CODEC_SCL  : U1.G42, U7.SCL
CODEC_NC   : U7.MCK, U7.SPK+, U7.SPK-, U7.IO, U7.MIC, U7.BIAS

AUX_L_JACK : J4.tip, R_AUX_L_100K.1, C_AUX_L_2U2.1
AUX_R_JACK : J4.ring, R_AUX_R_100K.1, C_AUX_R_2U2.1
             (R_AUX_L_100K.2, R_AUX_R_100K.2 → GND)
AUX_L_AC   : C_AUX_L_2U2.2, R_AUX_L_10K.1
AUX_R_AC   : C_AUX_R_2U2.2, R_AUX_R_10K.1
CODEC_AIN1 : R_AUX_L_10K.2, U7.AIN1
CODEC_AIN2 : R_AUX_R_10K.2, U7.AIN2
             (J4.sleeve → GND)
```

검토 규칙:

- U7 `VIN=+5V`, `GND=GND`; I2S BCK/WS/DIN은 5절 네트를 사용한다.
- AUX 100kΩ은 커플링 콘덴서의 잭 쪽에만 둔다. 내부 common-mode로 bias되는 AIN1/AIN2를
  저항으로 GND에 당기지 않는다.
- 2.2uF와 10kΩ/코덱 명목 11.2kΩ 입력으로 DC를 차단하고 +4dBu급 AUX 입력을 감쇠한다.
- 헤드폰은 U7 온보드 AC-coupled 3.5mm 잭을 사용한다. speaker output은 배선·펌웨어 모두 끈다.
- 앱/AUX 경로는 메인 기타 J1→J2 Thru와 전기적으로 섞지 않는다.

---

## 9. MIDI TRS-A IN/OUT

TRS-A는 `tip=DIN pin 5/current sink`, `ring=DIN pin 4/current source`,
`sleeve=DIN pin 2/shield`다.

```
MIDI_TX_A1 : U1.G6, U8.1
MIDI_TX_A2 : U8.2, U8.3
MIDI_OUT_5 : U8.4, R_MIDI_OUT_220_SINK.1
J6.tip     : R_MIDI_OUT_220_SINK.2
J6.ring    : R_MIDI_OUT_220_SOURCE.1
             (R_MIDI_OUT_220_SOURCE.2 → +5V, J6.sleeve → GND)
U8_POWER   : U8.14→+5V, U8.7→GND, C_MIDI_OUT_100N between U8.14/U8.7
U8_UNUSED  : U8.5/U8.9/U8.11/U8.13→GND; U8.6/U8.8/U8.10/U8.12→NC

MIDI_IN_4  : J5.ring, R_MIDI_IN_220.1
MIDI_LED_A : R_MIDI_IN_220.2, U9.2, D_MIDI_REV.cathode
MIDI_LED_K : U9.3, D_MIDI_REV.anode, J5.tip
MIDI_RX    : U9.6, U1.G5, R_MIDI_RX_2K2.2
U9_BASE    : U9.7, R_MIDI_BASE_4K7.1 (R_MIDI_BASE_4K7.2→GND)
U9_POWER   : U9.8→+5V, U9.5→GND, C_MIDI_IN_100N between U9.8/U9.5
U9_NC      : U9.1, U9.4
MIDI_IN_ISO: J5.sleeve→NC; J5 tip/ring/sleeve에 GND DC 경로 없음
```

검토 규칙: U8은 SN74AHCT14N이며 5V에서 3.3V GPIO high를 TTL high로 인식한다. 두 gate를
연속 사용해 UART 극성을 유지한다. U9는 6N138이다. J5는 절연형 잭을 써서 금속 외함이나
오디오 GND와 닿지 않게 한다.

---

## 10. 메인 Thru 뮤트 — AQY221R2S 병렬 shunt

```
MUTE_CTL   : U1.G39, R_MUTE_390.1
MUTE_LED_A : R_MUTE_390.2, U10.1
MUTE_LED_K : U10.2, GND
MUTE_SHUNT : U10.3, J2.tip
MUTE_RETURN: U10.4, J2.sleeve, GND
NC         : U1.G3, J201 전체
```

검토 규칙: U10은 1 Form A AC/DC PhotoMOS다. 전원 OFF에서 열리고, G39 high일 때만
J2 tip-sleeve를 약 1Ω으로 닫는다. 출력 pin 3/4 방향은 바뀌어도 된다. J1.tip과 J2.tip의
직결선에는 어떤 부품도 삽입하지 않는다. G3는 ESP32-S3 strapping 핀이므로 비운다.

---

## 11. 핀 사용 요약 (중복·금지핀 검토용)

목표 기능에 사용: 1,2,4,5,6,7,8,9,10,11,12,13,14,17,18,21,39,40,41,42,47
- 현재 비움: 3,15,16
- 금지핀 회피: 0·45·46(스트래핑, 비움), 19·20(USB), 26~37(옥타 플래시/PSRAM), 38·48(RGB LED).
- 스트래핑 3을 뮤트에서 제거했다. 12는 S3에선 일반 IO(구형 ESP32와 다름).
- 각 GPIO는 **정확히 하나의 기능**에만. SD의 11·47은 현재 배선돼 있다.

---

## 리뷰 시 내가 확인하는 것 (체크리스트)
- [ ] 각 네트 끝점이 위 스펙과 일치(핀 번호·부품)
- [ ] `+5V` ↔ `+3V3` 미연결, 전원 단일 소스
- [ ] D1 방향 RAW→PROT, U11 VIN1=BUCK_5V/VOUT=+5V/GND 공통, VIN2·OFF·STAT=NC
- [ ] `SW_GAIN`, `GAIN_LINE`, `GAIN_INST`, 구형 TL072 네트가 없음
- [ ] `U4.6=U4.7`(버퍼), VREF=`+9V_OPAMP/2`
- [ ] `U6.2=U6.1`(HOT 버퍼), `U6.6=U6.7` 및 `U6.5=VREF`(미사용 B 종단)
- [ ] PCM `VINL=HOT`, `VINR=SENSITIVE`, 두 입력 상호 단락 없음
- [ ] BAV199 `pin1=GND`, `pin2=+9V_OPAMP`, `pin3=SENSE_P`
- [ ] HOT `10M||3.3pF`, `1.5M||15pF`와 C0G 튜닝 패드
- [ ] 커플링 캡 ≥1µF(저역 보존)
- [ ] LCD/SD SCLK·MOSI 버스 공유, CS 분리, J3.VCC=`+5V`
- [ ] U7 MCK·speaker 핀 미연결, BCK/WS 공유, G40=DIN, G41/G42=I2C, G7=RST
- [ ] AUX L/R 각각 2.2uF+10k 직렬, 100k는 잭 쪽, AIN 쪽 GND 저항 없음
- [ ] MIDI OUT Type A와 U8 두 gate 극성 유지, MIDI IN J5 전체 DC 절연
- [ ] PhotoMOS 출력은 J2 tip-sleeve 병렬이고 J1↔J2 직결선은 그대로 유지
- [ ] GPIO 중복 없음 / 금지핀 없음
- [ ] G39=뮤트, G3=NC
