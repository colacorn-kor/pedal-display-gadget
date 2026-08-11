# 오디오 프론트엔드 설계·검증 메모

> **대상:** Codex와 회로 설계 검토용. 현재 실물은 `hardware/AS_BUILT_WIRING.md`,
> 다음 권장 연결은 `ASSEMBLY.md`를 본다. 목표 회로의 핀 단위 SSOT는
> `hardware/NETLIST_SPEC.md`다.

## 1. 현재와 목표의 분리

| 구분 | 회로 | 상태 |
|---|---|---|
| 현재 실물 | TL072 관련 끝점 일부만 기록, 분석 구간 재배선 필요 | `AS_BUILT_WIRING.md` 확인 필요, 외부 9V 실측 전 |
| 다음 권장 상태 | OPA2192 2개, HOT/SENSITIVE 동시 캡처, PCM1808 VINL/VINR 자동 선택 | 부품 구매·별도 기판 실장·교정 대기 |

2026-08-10 사용자가 남은 하드웨어 전체 배선과 구매 목록 정리를 요청했다. 따라서
TL072+LINE/INST 임시 회로를 다시 만들지 않고 OPA2192 자동 듀얼레인지를 다음 조립
목표로 확정한다. 구매 목록은 `hardware/PURCHASE_LIST.md`, 배선은 `ASSEMBLY.md`를 따른다.
부품 실장과 무전원 검사를 통과할 때까지 기본 펌웨어는 `AUDIO_DUAL_RANGE=0`을 유지한다.

## 2. 변하지 않는 요구

- 입력잭 Tip과 출력잭 Tip은 직결한다. 메인 기타 신호는 전원, ADC, 펌웨어와 무관한
  하드와이어 Thru다.
- 분석 탭만 입력 신호를 복사하며 Thru 사이에 능동소자나 스위치를 넣지 않는다.
- 목표 분석 탭 입력 임피던스는 최소 5MΩ이며, 주파수 응답과 실제 입력 전압 환산값을
  20Hz~20kHz에서 교정한다.
- PCM1808의 두 채널을 동시에 샘플링하고 DSP가 overlap과 hysteresis로 사용할 범위를 고른다.
- `HOT_DIV`, `SENSE_P`와 고값 저항/pF 보상망은 솔더리스 브레드보드가 아니라 세척한
  만능기판 또는 PCB에 짧게 실장한다.

## 3. 기존 TL072 기준 측정 (선택)

기존 TL072 회로가 실제로 완성돼 있을 때만 비교 자료로 남긴다. 이 측정은 목표 회로
착수 조건이 아니다. 알려진 1kHz sine 신호원과
1kHz에서 정확도가 확인된 true-RMS 멀티미터 또는 오실로스코프를 사용한다. GG는 외부
9V만 연결하고 USB는 분리한다.

| 항목 | 측정 내용 |
|---|---|
| 무입력 | dB Meter `INPUT LINE`, `LIVE`에서 RMS와 sample peak를 30초 기록 |
| LINE | 100.0mVrms, 1kHz 입력의 `AVG 1s` Vrms와 sample peak dBFS |
| INST | 같은 입력과 표시 조건에서 Vrms와 sample peak dBFS |
| 입력 한계 | 각 범위에서 sample peak가 약 -1dBFS가 되는 입력 Vrms |
| Thru | 1kHz source 직결과 `GG INPUT -> THRU OUTPUT`의 전원 ON/OFF Vrms 차이 |

| 범위 | 무입력 | 100mVrms 표시 | peak 약 -1dBFS 입력 | 비고 |
|---|---:|---:|---:|---|
| LINE 2.00x |  |  |  |  |
| INST 7.82x |  |  |  |  |
| THRU 전원 OFF | 해당 없음 |  | 해당 없음 | source 대비 |
| THRU 전원 ON | 해당 없음 |  | 해당 없음 | source 대비 |

## 4. 목표 회로 착수 조건

다음 조건이 모두 충족되기 전에는 목표 회로에 전원을 넣거나 듀얼레인지 펌웨어를
플래시하지 않는다.

- OPA2192 dual 2개와 SOIC-8→DIP-8 어댑터 2개를 실제로 보유한다.
- BAV199, 22MΩ, 10MΩ, 1.5MΩ, 100kΩ, 30kΩ, 10kΩ, 100Ω 부품을 보유한다.
- 3.3pF/15pF와 1.0/1.5/2.2/4.7pF C0G/NP0 조정 부품을 보유한다.
- 1uF, 10nF, 100nF와 세척 가능한 소형 납땜 기판을 보유한다.
- `hardware/NETLIST_SPEC.md`의 U4/U6, BAV199 핀과 PCM `VINL=HOT`, `VINR=SENSITIVE`
  연결을 무전원 상태에서 대조할 수 있다.
- 입력 Tip↔출력 Tip 직결과 PhotoMOS 병렬 뮤트가 분석 탭과 독립임을 대조한다.

## 5. 목표 회로 검증 순서

### 무전원·전원 레일

- J1 Tip↔J2 Tip은 거의 0Ω이고 각 Tip↔GND는 단락이 아니어야 한다.
- `+9V_OPAMP`, `+5V`, `+3V3`는 GND와 단락이 아니어야 한다.
- PCM1808 VINL과 VINR은 서로 연결되지 않아야 한다.
- 외부 9V만 켠 상태에서 `+5V=4.9~5.1V`, `+3V3=3.2~3.4V`,
  `VREF≈+9V_OPAMP/2`인지 확인한다.
- 무신호에서 U4.1과 U6.1은 VREF 부근이어야 한다.

### 듀얼 펌웨어와 1kHz 교정

하드웨어 검사를 통과한 뒤에만 `AUDIO_DUAL_RANGE=1`로 빌드한다. dB Meter의
`Settings -> Mode -> Range Diagnostics`에서 HOT=VINL, SENSITIVE=VINR를 동시에 본다.

100.0mVrms, 1kHz 입력에서 raw `S/H`는 명목 약 `+29.7dB`다. 음수면 L/R 연결을 먼저
의심한다. 화면에 표시된 값을 `Vhot_shown`, `Vsens_shown`이라 하면 보정값은 다음과 같다.

```text
HOT correction_new       = correction_old * 0.1000 / Vhot_shown
SENSITIVE correction_new = correction_old * 0.1000 / Vsens_shown
```

두 INPUT RMS가 100mVrms ±1%, corrected mismatch가 ±0.1dB 이내면 1점 교정을 통과한다.

### 범위 전환과 주파수 응답

- 1kHz 레벨을 천천히 올릴 때 SENSITIVE→HOT 전환에서 Vrms와 그래프가 튀지 않아야 한다.
- 1.0Vrms에서 SENSITIVE clip은 허용하지만 HOT clip은 없어야 하고 ACTIVE는 HOT이어야 한다.
- HOT까지 clip이면 입력을 즉시 낮춘다. 실제 HOT clip에서 안전 마진을 뺀 값을 제품 최대
  입력으로 정한다.
- 100mVrms 일정 입력으로 20, 31.5, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 15k,
  20kHz를 측정한다.
- 먼저 HOT C0G 보상값으로 고역 편차를 줄이고, 남는 완만한 편차만 펌웨어 보정표 후보로
  기록한다. 솔더리스 브레드보드 측정값은 최종 LUT에 쓰지 않는다.
- 마지막으로 무입력 안정, 범위 일치, 전환 연속성, Tuner/Curve/Reference/dB Meter,
  I2S overflow 0회를 확인한다.

## 6. 설계 참고

- 목표 명목 이득: SENSITIVE 약 3.98x, HOT 약 0.1304x
- PCM1808 3.0Vpp 기준 sine 입력 한계: SENSITIVE 약 0.265Vrms, HOT 약 8.13Vrms
- HOT 첫 보상값: `10MΩ || 3.3pF` 대 `1.5MΩ || 15pF`
- 데이터시트: [TI OPA2192](https://www.ti.com/lit/ds/symlink/opa2192.pdf),
  [TI PCM1808](https://www.ti.com/lit/ds/symlink/pcm1808.pdf),
  [Nexperia BAV199](https://assets.nexperia.com/documents/data-sheet/BAV199.pdf)
