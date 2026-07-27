# TAEYUN_TODO.md - 태윤의 현재 물리 작업

> 소프트웨어와 실기 상태는 `LAB_STATE.md`, 전체 우선순위는 `PUNCHLIST.md`를 따른다.
> 아래 항목이 끝나면 완료 항목을 지우고, 파일이 비면 이 문서를 삭제한다.

## 1. KiCad 풋프린트 배치

1. `hardware/pedal-display-gadget.pretty/` 폴더를 만든다.
2. 다음 `.kicad_mod` 4개를 넣는다.
   - `ESP32-S3-DevKitC-1`
   - `Jack_6.35mm_Mono_Panel`
   - `MP1584_Module`
   - `PWR_ELB040202`
3. Git 변경 목록에 4개가 잡히는지 확인한다.
4. KiCad 심볼에서 `TL072_DIP8`와 `PWR_9V_ELB040202`가 보이는지 확인한다.

## 2. 다음 하드웨어 마일스톤

- `ASSEMBLY.md` Step 5A 기록표부터 채운다. USB를 분리하고 외부 9V만 사용해 현재
  TL072 LINE/INST의 무입력 noise, 100mVrms 1kHz gain, clip 근접 입력과 Thru를 측정한다.
- Step 5B 전에 OPA2192×2+SOIC 어댑터, BAV199, 22M/10M/1.5M/100k/30k/10k,
  3.3pF·15pF와 튜닝용 C0G 세트, 소형 만능기판의 보유 여부를 확인한다.
- 고값 저항과 pF 보상망은 솔더리스 브레드보드에 만들지 않는다. 사진과 Step 5A 기준을
  남긴 뒤 `ASSEMBLY.md` 5B-1부터 순서대로 진행한다.
- 외부 9V 연결 전 모든 GND 본딩과 USB/9V 동시 급전 금지를 다시 확인한다.
- USB-only 상태의 Tuner 음이름은 TL072 무전원 부유 입력이므로 오디오 기능 판정에서 제외한다.
- SD 카드 모듈과 뮤트 회로는 현재 미장착이며 별도 작업으로 남긴다.
- 코덱 모듈 선정은 급하지 않으며 S2 착수 전에 PCM5102A와 ES8388 중 결정한다.
