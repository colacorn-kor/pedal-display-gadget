# GG 하드웨어 구매표

> 이 문서는 목표 배선을 완성하기 위한 구매 기준이다. 실제 보유 여부는 주문 전에 다시
> 확인하고, 배선은 `ASSEMBLY.md`, 핀 단위 회로 SSOT는 `hardware/NETLIST_SPEC.md`를 따른다.
> 실물에 이미 연결된 상태는 이 문서가 아니라 `hardware/AS_BUILT_WIRING.md`가 권위다.

## 1. 이번에 새로 필요한 부품

### 전원 역류 차단

| 수량 | 부품 | 정확한 기준 | 용도 |
|---:|---|---|---|
| 1 | Schottky 다이오드 | **1N5822**, 축형, 3A 40V | D1 9V 역극성 보호 |
| 1 | 이상 다이오드 모듈 | **Adafruit LM66200 Product 5830** | USB→MP1584 역급전 차단, 2.5A/40mΩ |

기존 1N5819 대신 전류 여유가 큰 1N5822를 D1에 사용한다. MP1584 `OUT+`는 LM66200
`VIN1`, 모듈 `VOUT`은 시스템 `+5V`로 연결한다. `VIN2/OFF/STAT`은 비운다.

### 헤드폰 출력과 AUX 입력

| 수량 | 부품 | 정확한 기준 | 용도 |
|---:|---|---|---|
| 1 | I2S DAC/헤드폰 모듈 | **Adafruit TLV320DAC3100, Product 6309** | 앱 음원 재생, AUX 아날로그 믹스, 헤드폰 구동 |
| 1 | 3.5mm 스테레오 TRS 암잭 | 절연형 패널 마운트 권장 | AUX 입력 |
| 1 | 3.5mm TRS 연장 암잭 | 패널 마운트형, 선택 | 모듈의 온보드 헤드폰 잭을 외함으로 연장할 때만 사용 |
| 2 | 2.2uF 커플링 콘덴서 | 무극성 전해 또는 필름, 16V 이상 | AUX L/R DC 차단 |
| 2 | 10kΩ 저항 | 1%, 1/4W | AUX 최대 레벨 감쇠 |
| 2 | 100kΩ 저항 | 1%, 1/4W | AUX 잭 입력 기준점 |

TLV320DAC3100 모듈은 헤드폰 드라이버와 3.5mm 잭을 이미 포함하므로 별도 PCM5102A,
ES8388, 헤드폰 앰프는 사지 않는다. 스피커 출력은 이번 GG에서 사용하지 않는다.
AIN1/AIN2는 ESP가 읽는 ADC 입력이 아니라 헤드폰으로 직접 섞이는 아날로그 입력이다. AUX
녹음이나 스펙트럼 분석은 이번 구성의 범위가 아니다.

### MIDI IN/OUT

| 수량 | 부품 | 정확한 기준 | 용도 |
|---:|---|---|---|
| 2 | 3.5mm 스테레오 TRS 암잭 | **절연형** 패널 마운트 | MIDI IN, MIDI OUT |
| 2 | TRS-A↔DIN-5 MIDI 어댑터 | MIDI Association Type A | 일반 DIN MIDI 장비 연결 |
| 1 | 옵토커플러 | Broadcom **6N138**, DIP-8 | MIDI IN 갈바닉 절연 |
| 1 | Schmitt inverter | TI **SN74AHCT14N**, PDIP-14 | 3.3V UART를 표준 5V MIDI OUT으로 버퍼 |
| 1 | 1N4148 | 축형 소신호 다이오드 | 6N138 LED 역전압 보호 |
| 3 | 220Ω 저항 | 1%, 1/4W | MIDI IN 1개, MIDI OUT 2개 |
| 1 | 2.2kΩ 저항 | 1%, 1/4W | MIDI RX pull-up |
| 1 | 4.7kΩ 저항 | 1%, 1/4W | 6N138 응답 속도 조정 |
| 2 | 100nF 세라믹 | 16V 이상 | 6N138, SN74AHCT14 전원 디커플링 |
| 1 | DIP-8 소켓 | 선택 | 6N138 교체 편의 |
| 1 | DIP-14 소켓 | 선택 | SN74AHCT14 교체 편의 |

MIDI IN 잭은 Sleeve까지 회로 GND와 절연해야 하므로 금속 외함에 자동 접촉하는 잭을 피한다.

### 메인 기타 Thru 뮤트

| 수량 | 부품 | 정확한 기준 | 용도 |
|---:|---|---|---|
| 1 | 저용량 PhotoMOS | Panasonic **AQY221R2S**, SOP-4 | 전원 OFF 때 열린 상태인 병렬 뮤트 |
| 1 | SOP-4→DIP 어댑터 | 2.54mm 브레드보드/만능기판용 | AQY221R2S 실장 |
| 1 | 390Ω 저항 | 1%, 1/4W | PhotoMOS LED 약 5mA 구동 |

`AQY221R2S`를 구하기 어렵다면 같은 1 Form A AC/DC 구조이며 출력 정전용량이 20pF 이하인
부품만 대체 후보로 삼는다. 대체품의 핀 배열을 확인하기 전에는 배선하지 않는다. J201은
이번 회로에서 사용하지 않는다.

## 2. 자동 듀얼레인지 분석 탭 부품

현재 TL072+LINE/INST 임시 회로를 다시 만들지 않고 아래 목표 회로로 진행한다. 고값 저항과
pF 보상망은 솔더리스 브레드보드가 아니라 세척 가능한 만능기판 또는 PCB에 실장한다.

| 수량 | 부품 | 규격 |
|---:|---|---|
| 2 | OPA2192 | OPA2192IDR 등 SOIC-8 dual op-amp |
| 2 | SOIC-8→DIP-8 어댑터 | OPA2192용 |
| 1 | BAV199 | 저누설 dual-series 다이오드, 발주 패키지용 어댑터 포함 |
| 1씩 | 정밀 저항 | 22MΩ, 10MΩ, 1.5MΩ, 100kΩ, 30kΩ |
| 3 이상 | 정밀 저항 | 10kΩ |
| 3 이상 | 저항 | 100Ω |
| 1씩 | C0G/NP0 | 3.3pF, 15pF |
| 각 2 이상 | C0G/NP0 조정용 | 1.0pF, 1.5pF, 2.2pF, 4.7pF |
| 3 | 1uF 커플링 콘덴서 | 필름 또는 무극성, 16V 이상 |
| 2 | 10nF 콘덴서 | 필름 또는 C0G 권장 |
| 4 이상 | 100nF 세라믹 | 전원/VREF 디커플링 |
| 2 | 100uF 전해 | 16V 이상 |
| 1 | 소형 만능기판 | 세척 가능, 고임피던스 구간 전용 |

저항과 콘덴서는 교정 중 값을 바꿀 수 있으므로 한 개씩만 주문하지 말고 여분을 둔다.

## 3. 이미 있는 것으로 간주하되 확인할 것

| 부품 | 현재 문서상 상태 |
|---|---|
| ESP32-S3-DevKitC-1, ST7796, PCM1808 | 실물 연결 완료 |
| SZH-EKBZ-005 MicroSD 모듈 | 실물 배선 완료, 기능 확인 대기 |
| MP1584, ELB040202 | 현재 전원 회로 구성품. 1N5819는 목표 회로에서 1N5822로 교체 |
| 6.35mm 입력/출력 잭 | Tip 직결 Thru와 Sleeve 공통 GND 구성 |
| TRS 6키 잭·저항, 풋스위치 | 실물 연결 완료 |

## 4. 이번 구성에서 사지 않는 것

| 부품 | 이유 |
|---|---|
| PCM5102A | 헤드폰 드라이버와 AUX 믹스 입력이 없어 TLV320DAC3100으로 대체 |
| ES8388 | PCM1808 분석 ADC를 유지하므로 통합 ADC 코덱이 불필요 |
| 별도 헤드폰 앰프 | TLV320DAC3100 모듈에 포함 |
| J201 | 음전압 게이트가 필요하고 하드와이어 Thru 원칙과 맞지 않음 |
| LINE/INST SPDT | 자동 듀얼레인지 목표 회로에서 사용하지 않음 |
| Bluetooth 오디오 모듈 | GG 제품 범위에서 제외 |

## 5. 부품 근거

- [Adafruit TLV320DAC3100 핀 설명](https://learn.adafruit.com/adafruit-tlv320dac3100-i2s-dac/pinouts)
- [TI TLV320DAC3100](https://www.ti.com/product/TLV320DAC3100)
- [MIDI 1.0 Electrical Specification Update](https://www.midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf)
- [MIDI TRS Type A 배선](https://midi.org/updated-how-to-make-your-own-3-5mm-mini-stereo-trs-to-midi-5-pin-din-cables)
- [Broadcom 6N138](https://www.broadcom.com/products/optocouplers/industrial-plastic/digital-optocouplers/100kbd/6n138)
- [TI SN74AHCT14](https://www.ti.com/product/SN74AHCT14)
- [Panasonic AQY221R2S](https://na.industrial.panasonic.com/products/relays-contactors/semiconductor-relays/series/12930/model/12938)
- [TI OPA2192](https://www.ti.com/lit/ds/symlink/opa2192.pdf)
- [MPS MP1584 데이터시트](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584EN-LF-Z/document_id/204/)
- [Adafruit LM66200 Product 5830](https://www.adafruit.com/product/5830)
