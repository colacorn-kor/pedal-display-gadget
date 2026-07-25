# TAEYUN_TODO.md - 태윤의 현재 물리 작업

> 소프트웨어와 실기 상태는 `LAB_STATE.md`, 전체 우선순위는 `PUNCHLIST.md`를 따른다.
> 아래 항목이 끝나면 완료 항목을 지우고, 파일이 비면 이 문서를 삭제한다.

## 1. TRS 6키 묶음 실측

- Codex가 COM4 기록을 시작하면 UP, DOWN, LEFT, RIGHT, OK, HOME을 각 3초씩 누른다.
- 이어서 HOME 길게와 FOOTSW 길게를 각각 1회 확인한다.
- 개별 확인을 여러 번 요청하지 않고 이 한 번의 입력 순서로 ADC 보정 자료를 수집한다.

## 2. KiCad 풋프린트 배치

1. `hardware/pedal-display-gadget.pretty/` 폴더를 만든다.
2. 다음 `.kicad_mod` 4개를 넣는다.
   - `ESP32-S3-DevKitC-1`
   - `Jack_6.35mm_Mono_Panel`
   - `MP1584_Module`
   - `PWR_ELB040202`
3. Git 변경 목록에 4개가 잡히는지 확인한다.
4. KiCad 심볼에서 `TL072_DIP8`와 `PWR_9V_ELB040202`가 보이는지 확인한다.

## 3. 다음 하드웨어 마일스톤

- 다음 별도 단계에서 조립된 TL072 오디오 입력 프론트엔드를 실기 검증한다.
- 외부 9V 연결 전 모든 GND 본딩과 USB/9V 동시 급전 금지를 다시 확인한다.
- USB-only 상태의 Tuner 음이름은 TL072 무전원 부유 입력이므로 오디오 기능 판정에서 제외한다.
- SD 카드 모듈과 뮤트 회로는 현재 미장착이며 별도 작업으로 남긴다.
- 코덱 모듈 선정은 급하지 않으며 S2 착수 전에 PCM5102A와 ES8388 중 결정한다.
