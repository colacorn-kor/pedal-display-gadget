# PUNCHLIST.md - 미결 및 확인 항목

> 현재 장치 상태와 실기 절차는 `LAB_STATE.md`가 권위다.

## P0 - 현재 차단 요인

- 없음

## P1 - 다음 실측

1. **Step 5A 구형 LINE/INST 기준 측정**
   - USB 분리·외부 9V 단독, 무입력 noise
   - 100mVrms 1kHz의 LINE/INST 표시와 clip 근접 입력
   - 전원 ON/OFF 하드와이어 Thru 레벨 비교
   - 상세 순서와 기록표는 `ASSEMBLY.md` Step 5A

## P2 - 하드웨어 및 제품 튜닝

2. KiCad `.pretty` 파일 4개 배치와 추적 확인(`TAEYUN_TODO.md` 참조)
3. 커스텀 풋프린트 실측: DevKit 행간, 6.35mm 잭, MP1584, 9V 커넥터
4. 조립된 TL072 오디오 입력의 9V 전원 실기 검증과 노이즈 저감
5. 튜너 5분 무리셋 및 I2S overflow 로그 실기 검증
6. 현재 LINE 2.00x / INST 7.82x 구형 프론트엔드의 기준 실측
7. `CONFIG_LV_DEF_REFR_PERIOD=33ms`에서 15ms 변경 여부 결정
8. 코덱 모듈 선정: PCM5102A와 ES8388 중 선택
9. 실오디오 연결 후 온셋 임계 1.8배·불응 80ms 튜닝
10. 외부 9V와 실제 기타 입력에서 Curve/Reference 주파수 응답·피크 감쇠 실기 튜닝
11. `AUDIO_DUAL_RANGE=1` L/R 자동 선택의 Step 5B 실기 검증
   - 자동 선택, Range Diagnostics, 호스트 테스트와 분리 빌드 소프트웨어 완료
   - 두 채널 overlap 일치, 실제 전환 연속성, clip 상태와 I2S overflow는 하드웨어 뒤 확인
12. Step 5B 자동 듀얼레인지 프론트엔드 조립
   - OPA2192 dual ×2, SENSITIVE 보호 clamp, HOT 보상 divider 적용
   - 고임피던스/pF 부분은 솔더리스 브레드보드가 아닌 세척한 납땜 기판 사용
   - HOT 시작값 `10M||3.3pF : 1.5M||15pF`, sweep 뒤 C0G 조정
13. 범위별 1kHz gain 및 20Hz~20kHz sweep 교정, GG Input Full Scale 확정
14. GG Analog Meter USB HID 보고서·바늘 ballistics·전력 예산 설계

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
- 앱 외형 소유권 분리: 전역 UI Theme은 런처·모든 공통 팝업, 앱 Color는 해당 앱만 변경.
  모든 앱에 `Default/Blue/White/Green` Color와 Mode 메뉴를 제공하며 Sound Monitor는
  3개 Mode, Bounce는 Classic Cat만 제공. NVS v5는 v2~v4 blob을 보존 마이그레이션
- 사용자 실기: 전역 Theme과 앱 팝업 팔레트 연동은 정상. Nyan은 장애물 등장 시
  프레임·입력 지연이 심해 제거
- `f2182fea`를 외부 9V 분리·USB 단독 상태에서 COM4에 플래시. 약 25초 부팅 로그에서
  8MB PSRAM 80MHz, 240MHz, ST7796/LVGL, ladder IDLE과 오류 0회 확인
- 사용자 실기: 공통 Color/Mode 화면과 Classic Cat 장애물 구간의 짧은
  HOME/FOOTSW 응답을 포함해 이상 없음
- Sound Monitor 개발본: Spectrum 표시명을 Curve로 변경하고 Curve 상하 기울기,
  좌우 DETAIL/BALANCED/SIMPLE, 별도 flat Reference 모드 구현. 시뮬레이터 smoke 통과
- Curve/Reference 개발본: ESP-IDF 기본 `0xd2040`·래더 비활성 `0xce9c0`,
  호스트 4/4, COM4 일반 플래시와 25초 무오류 부팅 로그 통과
- Basic 래더는 판정창 변경 없이 6키 UI 동작이 다시 정상임을 사용자가 확인했다.
  전압 재수집은 하지 않았으며 앞선 이탈은 일시적 접점 변화로 관찰한다.
- 사용자 실기에서 Curve tilt·단순화 조작과 Reference `FLAT`, 짧은 HOME/FOOTSW가
  모두 정상 동작했다. USB-only 무입력의 20~70Hz 약 -65dBFS 성분은 TL072 무전원
  부유 입력의 60Hz 주변 험으로 분류하며 외부 9V 실오디오 시험 전에는 성능 판정에서 제외한다.
