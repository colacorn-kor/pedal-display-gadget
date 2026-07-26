# PUNCHLIST.md - 미결 및 확인 항목

> 현재 장치 상태와 실기 절차는 `LAB_STATE.md`가 권위다.

## P0 - 현재 차단 요인

- 없음

## P1 - 다음 실측

1. **Basic 컨트롤러 체감 평가**
   - 6키, UP 연타, RIGHT+OK 데드존, 방향키 반복, HOME/FOOTSW 짧게·길게
   - 결과로 Smart 컨트롤러 착수 여부 결정
## P2 - 하드웨어 및 제품 튜닝

2. KiCad `.pretty` 파일 4개 배치와 추적 확인(`TAEYUN_TODO.md` 참조)
3. 커스텀 풋프린트 실측: DevKit 행간, 6.35mm 잭, MP1584, 9V 커넥터
4. 조립된 TL072 오디오 입력의 9V 전원 실기 검증과 노이즈 저감
5. 튜너 5분 무리셋 및 I2S overflow 로그 실기 검증
6. 게인 저항 튜닝: INST 2.2k / LINE 15k 시작점
7. `CONFIG_LV_DEF_REFR_PERIOD=33ms`에서 15ms 변경 여부 결정
8. 코덱 모듈 선정: PCM5102A와 ES8388 중 선택
9. 실오디오 연결 후 온셋 임계 1.8배·불응 80ms 튜닝
10. 외부 9V와 실제 기타 입력에서 Spectrum 주파수 응답·피크 감쇠 실기 튜닝
11. 알려진 1kHz Vrms 신호로 dB Meter LINE/INST 1점 전압 교정과 보정계수 확정

## 해결 및 관찰

- input WDT: 10ms 폴, 최소 1 tick, input prio3 적용 후 310초 무재발
- 정상 UI와 실제 FOOTSW 짧게 전환 확인
- TRS 6키 전부 판정창 통과, UP/DOWN 실측 간격 110mV
- HOME 롱은 Launcher, FOOTSW 롱은 Tuner 전환 확인
- 결정론적 simulator smoke CLI 2/2 통과, A#3 233.59Hz 잠금과 NVS 비변경 확인
- USB-only 오디오에서는 TL072가 꺼져 PCM1808 입력이 부유하므로 Tuner 판정을 신뢰하지 않음
- I2S 폭주 대응: DMA 8, 튜너 HOP 384, 로그 2초 제한, 64블록 yield 반영
- 저장소 위생: `build/`, `managed_components/`, 파생 `sdkconfig*` 추적 제거
- 사용자 확인: `ASSEMBLY.md` 조립 완료, SD 카드 모듈과 뮤트 회로만 미장착
- Ring 100Ω / Tip 220Ω은 조립 완료 범위에 포함
- SD VCC: 순수 어댑터 +3V3 직결로 확정
- NVS v1 -> v2 최초 부팅 시 1회 기본값 리셋은 정상
- 3행 런처: 빈 STASH 경유, Settings/Reorder 왕복, 선택 테두리와 대각 커서 실기 통과
- Sound Monitor: 20Hz~20kHz 로그 축, -72~0dBFS, 1kHz 기준 +4.5dB/oct,
  평활 현재선·채움·피크선 적용. PC 시각 검수와 COM4 정상 부팅 통과
- Sound Monitor 12밴드:
  50/100/200/400/600/800/1.2k/1.6k/3.2k/4.5k/6.4k/10kHz 적용
- 얼굴형 렌더러 제거, 고정 중심·PSRAM 기반 72-segment Circular spectrum으로 교체
- dB Meter 앱: LIVE 블록 또는 최근 1초/3초 전체 표본의 RMS 전력 평균, 5Hz 표시,
  sample peak dBFS와 1초 hold, 수동 LINE/INST 이득 기반 입력 잭 명목 Vrms·dBV·dBu 구현
- Bounce 앱: SPECTRUM 온셋 기반 고양이·종이컵 러너, 점수·충돌·온셋 재시작 구현
- 테마 소유권 분리: 전역 UI Theme은 런처·모든 공통 팝업, 앱 Theme은 해당 앱만 변경.
  Monitor 6프리셋과 Bounce Classic/Nyan Cat 선택을 앱별 저장. 현재 NVS는 dB Meter
  옵션 바이트를 더한 v4이며 v2/v3 blob을 보존 마이그레이션
