# PUNCHLIST.md - 미결 및 확인 항목

> 현재 장치 상태와 실기 절차는 `LAB_STATE.md`가 권위다.

## P0 - 현재 차단 요인

- 없음

## P1 - 다음 실측

1. **신회로 TRS 6키 판정표 재보정**
   - IDLE과 6키를 각 3초씩 기록하고 특히 UP 최대와 DOWN 최소 간격을 확인
   - 간격 40mV 미만이면 저항 재튜닝, 충분하면 실측 중심으로 window 확정
2. **Basic 컨트롤러 체감 평가**
   - 6키, UP 연타, RIGHT+OK 데드존, 방향키 반복, HOME/FOOTSW 짧게·길게
   - 결과로 Smart 컨트롤러 착수 여부 결정
3. **결정론적 PC 시뮬레이터 smoke CLI**
   - Windows 키 주입 없이 UI 이벤트·합성 오디오·앱 전환을 스크립트로 실행
   - 물리 버튼 요청 전에 공유 UI/DSP 경로를 자동 검증하는 용도

## P2 - 하드웨어 및 제품 튜닝

4. KiCad `.pretty` 파일 4개 배치와 추적 확인(`TAEYUN_TODO.md` 참조)
5. 커스텀 풋프린트 실측: DevKit 행간, 6.35mm 잭, MP1584, 9V 커넥터
6. 조립된 TL072 오디오 입력의 9V 전원 실기 검증과 노이즈 저감
7. 튜너 5분 무리셋 및 I2S overflow 로그 실기 검증
8. 게인 저항 튜닝: INST 2.2k / LINE 15k 시작점
9. `CONFIG_LV_DEF_REFR_PERIOD=33ms`에서 15ms 변경 여부 결정
10. 코덱 모듈 선정: PCM5102A와 ES8388 중 선택
11. 실오디오 연결 후 온셋 임계 1.8배·불응 80ms 튜닝

## 해결 및 관찰

- input WDT: 10ms 폴, 최소 1 tick, input prio3 적용 후 310초 무재발
- 정상 UI와 실제 FOOTSW 짧게 전환 확인
- USB-only 오디오에서는 TL072가 꺼져 PCM1808 입력이 부유하므로 Tuner 판정을 신뢰하지 않음
- I2S 폭주 대응: DMA 8, 튜너 HOP 384, 로그 2초 제한, 64블록 yield 반영
- 저장소 위생: `build/`, `managed_components/`, 파생 `sdkconfig*` 추적 제거
- 사용자 확인: `ASSEMBLY.md` 조립 완료, SD 카드 모듈과 뮤트 회로만 미장착
- Ring 100Ω / Tip 220Ω은 조립 완료 범위에 포함
- SD VCC: 순수 어댑터 +3V3 직결로 확정
- NVS v1 -> v2 최초 부팅 시 1회 기본값 리셋은 정상
