# GG 권장 배선표 (To-Be)

> 이 문서는 Codex가 제안하는 **다음 완성 상태의 배선표**다. 지금 실물에 실제로 연결된
> 상태는 `hardware/AS_BUILT_WIRING.md`가 권위다. 필요한 부품은
> `hardware/PURCHASE_LIST.md`, 회로 검토용 핀 단위 SSOT는 `hardware/NETLIST_SPEC.md`를 본다.

## 반드시 지킬 것

- **USB와 외부 9V를 동시에 연결하지 않는다.** 사용자가 별도로 알리지 않는 동안 외부
  9V는 분리된 상태로 본다.
- MP1584는 다른 보드에 연결하기 전에 `OUT+`가 `4.9~5.1V`인지 확인한다.
- TRS 컨트롤러 Ring은 `+3V3` 전용이다. `+5V`를 연결하지 않는다.
- 입력잭 Tip과 출력잭 Tip 사이에는 어떤 부품도 직렬로 넣지 않는다.
- J201은 사용하지 않고 G3도 비운다. 뮤트는 G39와 PhotoMOS로 구성한다.
- MIDI IN 잭의 Tip, Ring, Sleeve는 회로 GND와 직접 연결하지 않는다.
- 오디오 프론트엔드의 10MΩ/22MΩ/pF 노드는 솔더리스 브레드보드에 만들지 않는다.

## 1. 전원과 접지

| 네트 | 서로 연결할 점 |
|---|---|
| `+9V_RAW` | ELB040202 `+` ↔ D1 1N5822 애노드(띠 없는 쪽) |
| `+9V_PROT` | D1 1N5822 캐소드(띠 쪽) ↔ MP1584 `IN+` ↔ 100Ω 한쪽 |
| `+9V_OPAMP` | 위 100Ω 반대쪽 ↔ OPA2192 두 개 pin 8 ↔ 100uF `+` ↔ 각 100nF |
| `BUCK_5V` | MP1584 `OUT+` ↔ LM66200 모듈 `VIN1` |
| `+5V` | LM66200 모듈 `VOUT` ↔ DevKit `5V` ↔ PCM1808 `+5V` ↔ SD `VCC` ↔ TLV320DAC3100 `VIN` ↔ SN74AHCT14 pin 14 ↔ 6N138 pin 8 |
| `+3V3` | DevKit `3V3` ↔ ST7796 `VCC` ↔ PCM1808 `3.3` ↔ 컨트롤러 Ring 100Ω ↔ MIDI RX 2.2kΩ pull-up |
| `GND` | 전원 `-`, DevKit/디스플레이/PCM1808/SD/코덱 GND, op-amp pin 4, 오디오 잭 Sleeve, 컨트롤러 Sleeve, 풋스위치, MIDI OUT Sleeve, 6N138 pin 5, SN74AHCT14 pin 7 |

LM66200 모듈 `GND`는 GND에 연결한다. `VIN2`, `OFF`, `STAT`은 비운다. 모듈의 온보드
1MΩ pull-down 때문에 `OFF`를 비우면 기본 활성 상태다.

`+9V_OPAMP`의 100uF `-`와 100nF 반대쪽은 GND다. 100nF는 각 IC 전원핀 가까이에 둔다.
오디오 입력/출력 Sleeve와 코덱/AUX GND는 전원 입력부의 한 점으로 돌아가게 배선한다.
**MIDI IN 잭만 이 공통 GND 표에서 제외한다.**

LM66200은 USB의 5V가 전원이 꺼진 MP1584 출력으로 역급전되는 것을 막는다. D1의 띠는
부하 쪽이다.

외부 9V가 없고 USB만 연결된 동안 `+5V`와 `+3V3`는 켜지지만 OPA2192 분석 탭은 꺼져
있다. 이때 화면과 디지털 주변장치는 시험할 수 있지만 오디오 분석값은 유효하지 않다.

## 2. ESP32 GPIO 배정

| GPIO | 연결 대상 |
|---:|---|
| G1 | ST7796 `BL` |
| G2 | ST7796 `CS` |
| G3 | 연결하지 않음 |
| G4 | 220Ω을 거쳐 메인 컨트롤러 TRS `Tip` |
| G5 | MIDI IN, 6N138 pin 6 출력 |
| G6 | MIDI OUT, SN74AHCT14 pin 1 입력 |
| G7 | TLV320DAC3100 `RST` |
| G8 | PCM1808 `SCKI` |
| G9 | PCM1808 `BCK` + TLV320DAC3100 `BCK` 공유 |
| G10 | PCM1808 `OUT`/`DOUT` |
| G11 | SD `MISO` |
| G12 | ST7796 `SCL` + SD `SCK` 공유 |
| G13 | ST7796 `SDA` + SD `MOSI` 공유 |
| G14 | ST7796 `RST` |
| G17 | 풋스위치 |
| G18 | PCM1808 `LRC` + TLV320DAC3100 `WSEL` 공유 |
| G21 | ST7796 `DC` |
| G39 | PhotoMOS 메인 Thru 뮤트 |
| G40 | TLV320DAC3100 `DIN` |
| G41 | TLV320DAC3100 `SDA` |
| G42 | TLV320DAC3100 `SCL` |
| G47 | SD `CS` |

G15와 G16은 비운다. G5/G6을 MIDI에 쓰는 이 실물 구성은 TRS 저항 래더가 켜진 제품
펌웨어(`INPUT_TRS_LADDER=1`) 기준이다. 래더를 끈 구형 6개 직결 버튼 배선과는 동시에
사용하지 않는다.

## 3. ST7796 디스플레이

| ST7796 | 연결 |
|---|---|
| `GND` | GND |
| `VCC` | +3V3 |
| `SCL` | G12, SD SCK와 공유 |
| `SDA` | G13, SD MOSI와 공유 |
| `RST` | G14 |
| `DC` | G21 |
| `CS` | G2 |
| `BL` | G1 |
| `SDA-0` | 연결하지 않음 |

## 4. PCM1808 분석 ADC

| PCM1808 | 연결 |
|---|---|
| `+5V` | +5V |
| `3.3` | +3V3 |
| `GND` | GND |
| `SCKI` | G8 |
| `BCK` | G9, 코덱 BCK와 공유 |
| `OUT`/`DOUT` | G10 |
| `LRC`/`LRCK` | G18, 코덱 WSEL과 공유 |
| `VINL` | 아래 HOT 경로 출력 |
| `VINR` | 아래 SENSITIVE 경로 출력 |

PCM1808 모드 점퍼는 **ESP32 클럭을 받는 Slave + I2S 형식**으로 둔다.

## 5. MicroSD `SZH-EKBZ-005`

| SD 모듈 | 연결 |
|---|---|
| `GND` | GND |
| `VCC` | **+5V** |
| `MISO` | G11 |
| `MOSI` | G13, 디스플레이 MOSI와 공유 |
| `SCK` | G12, 디스플레이 SCK와 공유 |
| `CS` | G47 |

이 모듈은 4.5~5.5V VCC용 온보드 레귤레이터와 레벨 변환 회로를 사용한다. 카드는
전원을 끈 상태에서 넣거나 뺀다.

## 6. 컨트롤러와 풋스위치

### 메인 보드의 컨트롤러 TRS 잭

| TRS 접점 | 연결 |
|---|---|
| `Sleeve` | GND |
| `Ring` | 100Ω을 거쳐 +3V3 |
| `Tip` | 220Ω을 거쳐 G4 |

### 6키 컨트롤러 내부

| 연결 | 값 |
|---|---:|
| Ring ↔ Rtop ↔ Tip | 10kΩ |
| Tip ↔ UP 버튼 ↔ Sleeve | 0Ω |
| Tip ↔ DOWN 버튼 ↔ Sleeve | 470Ω |
| Tip ↔ LEFT 버튼 ↔ Sleeve | 1kΩ |
| Tip ↔ RIGHT 버튼 ↔ Sleeve | 2kΩ |
| Tip ↔ OK 버튼 ↔ Sleeve | 4.7kΩ |
| Tip ↔ HOME 버튼 ↔ Sleeve | 10kΩ |

### 풋스위치

| 접점 | 연결 |
|---|---|
| 평소 열려 있는 접점 한쪽 | G17 |
| 평소 열려 있는 접점 반대쪽 | GND |

## 7. 하드와이어 Thru와 자동 듀얼레인지 분석 탭

### 메인 오디오 잭

| 시작점 | 연결 | 도착점 |
|---|---|---|
| 입력잭 `Tip` | **직결** | 출력잭 `Tip` |
| 입력잭 `Sleeve` | 공통 GND | 출력잭 `Sleeve` |
| 입력잭 `Tip` | 1uF 무극성 커플링 | `ANALYZER_TAP` |

입력 Tip과 출력 Tip 사이에는 스위치, IC, 저항, 콘덴서를 넣지 않는다.

### VREF: 첫 번째 OPA2192의 B 채널

| 시작점 | 부품/연결 | 도착점 |
|---|---|---|
| `+9V_OPAMP` | 10kΩ | `VREF_DIV` |
| `VREF_DIV` | 10kΩ | GND |
| `VREF_DIV` | 100uF + 100nF 병렬 | GND |
| `VREF_DIV` | 직결 | U4 pin 5 |
| U4 pin 7 | 직결 | U4 pin 6 |
| U4 pin 7 | 이 점을 `VREF`로 사용 | 아래 두 분석 경로 |

`VREF`는 고정 4.5V가 아니라 실제 `+9V_OPAMP`의 절반이다. 9V 입력과 보호 다이오드의
전압 강하를 포함하면 보통 약 4.3V이며, 정확한 값은 전원 투입 뒤 측정한다.

### SENSITIVE 경로: PCM1808 VINR

| 시작점 | 부품/연결 | 도착점 |
|---|---|---|
| `ANALYZER_TAP` | 100kΩ | `SENSE_P` |
| `SENSE_P` | 22MΩ | `VREF` |
| `SENSE_P` | 직결 | U4 pin 3 |
| U4 pin 2 | 30kΩ | U4 pin 1 |
| U4 pin 2 | 10kΩ | `VREF` |
| U4 pin 1 | 1uF 무극성 → 100Ω 직렬 | PCM1808 `VINR` |
| PCM1808 `VINR` | 10nF | GND |

BAV199는 pin 1=GND, pin 3=`SENSE_P`, pin 2=`+9V_OPAMP`로 연결한다. 발주한 패키지의
데이터시트에서 핀 번호를 다시 확인한다.

### HOT 경로: PCM1808 VINL

| 시작점 | 부품/연결 | 도착점 |
|---|---|---|
| `ANALYZER_TAP` | 10MΩ과 3.3pF 병렬 | `HOT_DIV` |
| `HOT_DIV` | 1.5MΩ과 15pF 병렬 | `VREF` |
| `HOT_DIV` | 직결 | U6 pin 3 |
| U6 pin 1 | 직결 | U6 pin 2 |
| U6 pin 1 | 1uF 무극성 → 100Ω 직렬 | PCM1808 `VINL` |
| PCM1808 `VINL` | 10nF | GND |
| U6 pin 5 | 직결 | `VREF` |
| U6 pin 7 | 직결 | U6 pin 6 |

U4/U6 pin 8은 `+9V_OPAMP`, pin 4는 GND다. HOT의 10MΩ/1.5MΩ/pF 부품은 짧게
납땜하고 플럭스를 세척한다. 최종 pF 값은 sweep 교정 후 확정한다.

## 8. TLV320DAC3100, AUX와 헤드폰

### 코덱 모듈

| TLV320DAC3100 모듈 | 연결 |
|---|---|
| `VIN` | +5V |
| `GND` | GND |
| `BCK` | G9, PCM1808 BCK와 공유 |
| `WSEL` | G18, PCM1808 LRCK와 공유 |
| `DIN` | G40 |
| `SDA` | G41 |
| `SCL` | G42 |
| `RST` | G7 |
| `MCK` | 연결하지 않음. BCK를 PLL 입력으로 사용 |
| `AIN1` | AUX Left 경로 |
| `AIN2` | AUX Right 경로 |
| `SPK+`, `SPK-`, `IO`, `MIC`, `BIAS` | 연결하지 않음 |

### AUX 입력

| AUX TRS 접점 | 연결 |
|---|---|
| `Tip`(Left) | 100kΩ→GND를 달고, 2.2uF 무극성 → 10kΩ → 코덱 `AIN1` |
| `Ring`(Right) | 100kΩ→GND를 달고, 2.2uF 무극성 → 10kΩ → 코덱 `AIN2` |
| `Sleeve` | GND |

100kΩ은 커플링 콘덴서의 **잭 쪽**에 둔다. AIN1/AIN2 쪽에는 GND 저항을 달지 않는다.
10kΩ은 약 11.2kΩ인 코덱 입력과 함께 +4dBu급 입력도 여유 있게 받도록 감쇠한다.

헤드폰은 모듈의 온보드 3.5mm 잭에 꽂는다. 외함용 잭이 필요하면 완성된 스테레오 TRS
연장 케이블을 온보드 잭에 연결한다. 별도 헤드폰 앰프를 추가하지 않는다. 코덱의 스피커
출력은 펌웨어에서도 끈다.

이 헤드폰 경로는 앱 음원과 AUX를 위한 것이다. 메인 기타 Thru를 헤드폰으로 보내는 회로는
분석 탭을 부하시키지 않는 별도 버퍼/믹서가 필요하므로 이번 배선에는 포함하지 않는다.
AIN1/AIN2는 코덱 내부의 아날로그 믹스 입력이므로 AUX 소리를 ESP가 녹음하거나 분석하지는
않는다.

## 9. MIDI TRS-A IN/OUT

TRS-A 접점은 `Tip=DIN pin 5/current sink`, `Ring=DIN pin 4/current source`,
`Sleeve=DIN pin 2/shield`다.

### MIDI OUT: G6과 SN74AHCT14N

| 시작점 | 연결 | 도착점 |
|---|---|---|
| G6 | 직결 | SN74AHCT14 pin 1 |
| pin 2 | 직결 | pin 3 |
| pin 4 | 220Ω | MIDI OUT `Tip` |
| +5V | 220Ω | MIDI OUT `Ring` |
| MIDI OUT `Sleeve` | 직결 | GND |
| pin 14 / pin 7 | +5V / GND | IC 전원 |
| pin 14 | 100nF | pin 7 |

미사용 입력 pin 5, 9, 11, 13은 GND에 연결하고 미사용 출력 pin 6, 8, 10, 12는 비운다.
두 inverter를 연속 사용하므로 UART 극성은 유지된다.

### MIDI IN: 6N138

| 시작점 | 연결 | 도착점 |
|---|---|---|
| MIDI IN `Ring` | 220Ω | 6N138 pin 2(LED 애노드) |
| 6N138 pin 3(LED 캐소드) | 직결 | MIDI IN `Tip` |
| 1N4148 캐소드 / 애노드 | pin 2 / pin 3 | LED와 역병렬 보호 |
| 6N138 pin 8 / pin 5 | +5V / GND | 출력측 전원 |
| pin 8 | 100nF | pin 5 |
| pin 6 | 직결 | G5 |
| +3V3 | 2.2kΩ | pin 6 |
| pin 7 | 4.7kΩ | pin 5 |
| pin 1, pin 4 | 연결하지 않음 |  |
| MIDI IN `Sleeve` | **연결하지 않음** | 회로 GND와 DC 절연 유지 |

MIDI IN 잭은 절연형을 사용한다. 금속 외함을 쓸 때도 IN Sleeve와 잭 몸체가 회로 GND에
닿지 않게 한다.

## 10. 메인 Thru 뮤트

AQY221R2S는 출력잭에 병렬로 붙이는 normally-open PhotoMOS다. 전원이 없을 때 열려 있어
하드와이어 Thru에 직렬 소자가 남지 않는다.

| 시작점 | 연결 | 도착점 |
|---|---|---|
| G39 | 390Ω | AQY221R2S pin 1(LED 애노드) |
| AQY221R2S pin 2(LED 캐소드) | 직결 | GND |
| AQY221R2S pin 3 | 직결 | 출력잭 `Tip` |
| AQY221R2S pin 4 | 직결 | 출력잭 `Sleeve`/GND |

PhotoMOS 출력 pin 3/4는 AC/DC 양방향이라 서로 바뀌어도 된다. SOP-4 어댑터의 pin 1
표시를 먼저 확인한다. G39가 High면 Tip-Sleeve가 약 1Ω으로 닫혀 뮤트되고, Low 또는
전원 OFF면 열린다. G3와 J201은 연결하지 않는다.

## 11. 연결하지 않는 것

| 대상 | 상태 |
|---|---|
| G3, G15, G16 | 비움 |
| J201, LINE/INST SPDT, TL072 | 사용하지 않음 |
| ST7796 `SDA-0` | 비움 |
| TLV320DAC3100 `MCK`, `SPK+/-`, `IO`, `MIC`, `BIAS` | 비움 |
| MIDI IN Sleeve | 회로 GND와 DC 미연결 |

## 12. 전원 넣기 전 대조

| 확인점 | 정상 값/상태 |
|---|---|
| 입력 Tip ↔ 출력 Tip | 거의 0Ω |
| 입력/출력 Tip ↔ GND | 단락 아님. PhotoMOS는 전원 OFF에서 열림 |
| 각 전원 레일 ↔ GND | 단락 아님 |
| USB 단독, MP1584 `OUT+` | LM66200 때문에 시스템 `+5V`와 DC 직결되지 않음 |
| +5V ↔ +3V3 | 서로 직접 연결되지 않음 |
| PCM1808 VINL ↔ VINR | 서로 연결되지 않음 |
| AUX Tip/Ring ↔ AIN1/AIN2 | 2.2uF 커플링 때문에 DC 직결 아님 |
| MIDI IN Sleeve ↔ GND | DC 미연결 |
| MIDI IN Tip/Ring ↔ GND | DC 미연결 |
| SD VCC | +5V만 연결 |
| TRS 컨트롤러 Ring | 100Ω을 거쳐 +3V3 |

외부 9V만 연결한 뒤 `+9V_PROT≈8.6V`, `+5V=4.9~5.1V`, `+3V3=3.2~3.4V`,
`VREF≈+9V_OPAMP/2`를 확인한다. 코덱과 MIDI의 실제 기능은 ESP 백엔드가 추가된 펌웨어를
플래시한 뒤 시험한다. 뮤트는 G39를 사용하는 펌웨어부터 동작한다.
