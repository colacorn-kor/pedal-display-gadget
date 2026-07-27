# NETLIST_SPEC — 스키매틱 기준 연결 스펙 (SSOT)

KiCad로 그릴 때 이 네트들을 그대로 구현하고, `File → Export → Netlist…`로 뽑은 `.net`을
이 문서와 대조(diff)한다. 형식: `NET_NAME: 끝점1, 끝점2, …` (부품.핀 표기).

> **리비전 상태:** 아래 권위 넷은 GG 목표인 **무스위치 자동 듀얼레인지** 회로다. 현재
> 브레드보드에는 아직 구형 LINE/INST SPDT 회로가 장착돼 있으며, 실제 상태와 개조 전제는
> `ASSEMBLY.md` Step 5/5A/5B와 `LAB_STATE.md`에 기록한다. Step 5A 기준 측정과 Step 5B
> 무전원 검사를 건너뛰고 개조하거나 듀얼 펌웨어를 플래시하지 않는다.
>
> 참조 부품(레퍼런스 예시): `U1`=ESP32-S3-DevKitC-1, `U2`=ST7796 디스플레이,
> `U3`=PCM1808 모듈, `U4/U6`=OPA2192 RRIO dual op-amp, `U5`=MP1584, `D1`=1N5819,
> `D2`=BAV199 저누설 dual-series clamp,
> `J1`=입력잭, `J2`=출력잭,
> `J3`=SD 어댑터, `SW1..SW7`=버튼,
> `PWR1`=ELB040202(9V 입력).
> 저항/캡은 값으로 부른다(R10k_1 등). OPA2192 핀번호는 표준 dual op-amp
> 8핀 기준(1=OUTA,2=-INA,3=+INA,4=V−,5=+INB,6=−INB,7=OUTB,8=V+).
> 현재 DIP TL072와 핀 기능은 같지만 최종 패키지와 footprint는 발주 부품에 맞춘다.

---

## 1. 전원 네트

```
+9V_RAW    : PWR1.+, D1.anode
+9V_PROT   : D1.cathode(띠), U5(MP1584).IN+, R100.1        ← 역전압 보호 뒤. 부하는 전부 여기서
GND        : (모든 GND 공통 — PWR1.-, U5.IN-/OUT-, U1.GND, U2.GND, U3.GND,
             U4.4(V−), U6.4(V−), D2.1(A1),
             D-잭 슬리브, 각 디커플링 캡의 GND쪽 …)  ★ 스타 그라운드 한 점
+5V        : U5(MP1584).OUT+, U1(DevKit).5V, U3(PCM1808).+5V
+3V3       : U1(DevKit).3V3, U3(PCM1808).3.3, U2(ST7796).VCC
```

op-amp 전용 깨끗한 9V (RC 필터):
```
+9V_OPAMP  : R100.2, U4.8(V+), U6.8(V+), C100u_op.+,
             C100n_op.1, C100n_u6.1, D2.2(K2)
             (R100 = 100Ω 직렬,  C100u_op = 100µF 전해,  C100n_op = 100nF 세라믹, 둘 다 →GND)
```

검토 규칙: `+5V`와 `+3V3`는 **절대 서로 연결 금지**. `+9V_PROT` 이후에만 부하. 다이오드
방향은 `D1.anode=+9V_RAW / D1.cathode=+9V_PROT` (띠가 부하 쪽).

---

## 2. 가상 그라운드 4.5V (U4 B쪽 버퍼)

```
VREF_DIV   : R10k_a.2, R10k_b.1, C100u_ref.+, C100n_ref.1, U4.5(+INB)
             (R10k_a: +9V_OPAMP↔VREF_DIV,  R10k_b: VREF_DIV↔GND → 분압 4.5V)
             (C100u_ref, C100n_ref → GND)
VREF       : U4.7(OUTB), U4.6(−INB), R22M_sense.2,
             R10k_sense.2, R1M5_hot.2, C15p_hot.2, U6.5(+INB)
             ← 버퍼된 저임피던스 4.5V
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

## 5. I2S 오디오 — U1 ↔ U3

```
I2S_MCLK   : U1.G8,  U3.SCKI
I2S_BCK    : U1.G9,  U3.BCK
I2S_WS     : U1.G18, U3.LRC
I2S_DIN    : U1.G10, U3.OUT(DOUT)
```

검토 규칙: U3 모드 점퍼 = 슬레이브/ I2S(스키매틱 주석으로 표기). U3 `+5V`/`3.3` 분리 확인.

---

## 6. SD 어댑터 (선택 장착, SDSPI/FATFS 구현) — U1 ↔ J3

```
LCD_SCLK   : (+ J3.SCK)          ← 4절 SCLK 네트에 J3.SCK 추가(버스 공유)
LCD_MOSI   : (+ J3.MOSI)         ← 4절 MOSI 네트에 J3.MOSI 추가
SD_MISO    : U1.G11, J3.MISO     ← SD 전용
SD_CS      : U1.G47, J3.CS       ← SD 전용
             J3.VCC → +3V3(순수 어댑터),  J3.GND → GND
```

> 즉 `LCD_SCLK`·`LCD_MOSI`는 U2와 J3가 **함께 매달린 한 네트**. CS만 각자(LCD=G2, SD=G47).

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

GPIO_RESERVED  : U1.G5, U1.G6, U1.G7, U1.G15, U1.G16
                 (미사용, Phase 2 예비)
FOOTSW          : U1.G17, SW7.1 (SW7.2 → GND)
```

검토 규칙: 기존 본체 GPIO 버튼 6개는 제거한다. G4의 `TRS_SIG`는 Basic에서 ADC 입력,
Smart에서 디지털 통신으로 재사용하며 과도한 RC 필터를 달지 않는다. Rtop 10k는 분압과
Smart 오픈드레인 풀업을 겸한다. Ring 전압은 **+3V3 고정이며 5V 연결 금지**다. Tip 직렬
220Ω은 G4를 보호하고, Ring 직렬 100Ω은 TS 플러그 삽입 시 Ring-Sleeve 단락 전류를
제한한다. G5/G6/G7/G15/G16은 연결하지 않는다. FOOTSW만 내부 풀업을 쓰는 active-low
GPIO 입력이다.

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
- 각 GPIO는 **정확히 하나의 기능**에만. SD의 11·47은 선택 장착 시 활성화된다.

---

## 리뷰 시 내가 확인하는 것 (체크리스트)
- [ ] 각 네트 끝점이 위 스펙과 일치(핀 번호·부품)
- [ ] `+5V` ↔ `+3V3` 미연결, 전원 단일 소스
- [ ] `D1` 방향(anode=RAW / cathode=PROT)
- [ ] `SW_GAIN`, `GAIN_LINE`, `GAIN_INST`, 구형 TL072 네트가 없음
- [ ] `U4.6=U4.7`(버퍼), 분압 4.5V
- [ ] `U6.2=U6.1`(HOT 버퍼), `U6.6=U6.7` 및 `U6.5=VREF`(미사용 B 종단)
- [ ] PCM `VINL=HOT`, `VINR=SENSITIVE`, 두 입력 상호 단락 없음
- [ ] BAV199 `pin1=GND`, `pin2=+9V_OPAMP`, `pin3=SENSE_P`
- [ ] HOT `10M||3.3pF`, `1.5M||15pF`와 C0G 튜닝 패드
- [ ] 커플링 캡 ≥1µF(저역 보존)
- [ ] LCD/SD SCLK·MOSI 버스 공유, CS 분리
- [ ] GPIO 중복 없음 / 금지핀 없음
- [ ] MUTE(G3)는 드라이버 회로 확정 전까지 미배선
