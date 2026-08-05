# PUNCHLIST.md - 미결 및 확인 항목

> 현재 장치 상태와 실기 절차는 `LAB_STATE.md`가 권위다.

## P0 - 현재 차단 요인

- 없음

## P1 - PC 기준 제품

1. **D0A Sound Monitor 우선 최적화**
   - 완료: Curve/Reference Slope 0, 중심 정렬 3단계 FFT, DC 차단, Reference 무잔상,
     30/300/3000Hz·경계 sweep 회귀 검사, Flat/A-weighted와 60-phon Loudness 표시
   - 남음: 12-Band·Circular의 프레임 시간과 입력 응답 계측
2. **D0 기존 UI·설정 기준선 감사**
   - Launcher, Sound Monitor, Gallery, Tuner, Bounce, dB Meter의 모든 화면·빈 상태·오류 상태 목록화
   - 전역 Dark/Light·Color와 앱 Color/Mode·세부 설정의 실제 값·표시·NVS 복원 일치 확인
   - 앱별 preview와 smoke 수용 기준 확장
3. **D1 플랫폼 능력과 PC 오디오 출력 기반**
   - `needs_codec` 하드코딩을 `AUDIO_PLAYBACK_OUTPUT` 플랫폼 능력 검사로 일반화
   - 공통 재생 transport·믹서 API와 PC SDL 출력 백엔드 구현
   - 코덱 없는 ESP는 명시적 unavailable 상태로 같은 공통 앱 소스를 계속 빌드
4. **D2 Music 앱**
   - `GG/music` 브라우저, WAV 재생/일시정지, 이전/다음, 진행률, 볼륨, 오류 상태
   - 파일이 없을 때 내장 8비트 로비 음악 재생, 앱 종료 시 정지
   - MP3/FLAC/OGG는 메모리·라이선스·ESP 이식성 확인 뒤 공통 디코더 경계에 추가
5. **D3 Metronome과 앱 효과음**
   - BPM·박자·subdivision·accent UI와 click 출력
   - Bounce 등 공통 앱 효과음을 같은 믹서에 연결
6. **D4 Gallery 미디어 UX**
   - 어두운 `GG` 빈 화면 폴백·손상 파일·긴 파일명·로딩·정렬·새로고침 상태 완성
7. **D5 Game 앱과 실행**
   - 공통 게임 루프와 입력/오디오 출력 API 정리
   - 파일 없음=`No Game`, 무선택 또는 `No Game`에서 OK/Play=내장 점프 게임
   - GG에서 가능한 Retro-Go급 코어만 PC와 동일 어댑터로 실제 실행
   - GBA/NDS는 GG2 범위로 유지
8. **Setlist와 MIDI Monitor**
   - PC MIDI 입출력 백엔드와 앱 UI를 먼저 구현하고 ESP UART/BLE에 이식

## P2 - PC 앱 완성도와 지속 검증

9. 시스템 오디오·마이크·미디어 폴더·출력 장치 선택을 앱 UI에서도 제공
10. 창 배율·전체화면·게임패드 입력과 배포 가능한 실행 패키지 정리
11. 모든 새 공통 기능에 preview/smoke/호스트 테스트와 ESP 기본·래더 비활성 빌드 유지
12. 삭제된 Python 3.14 경로를 가리키는 ESP-IDF v5.4.4 공식 Python 환경 복구

## P3 - 병행 하드웨어

13. **실물 As-Built 오디오 기록과 TL072 재배선**
   - 현재 오디오 끝점을 직접 확인해 `hardware/AS_BUILT_WIRING.md`의 미확인 행 갱신
   - `ASSEMBLY.md` 권장안 반영 뒤 As-Built를 다시 현재 상태로 갱신
   - 전원 OFF에서 Thru 연속성·레일 단락 확인
   - USB 분리·외부 9V 단독으로 +5V/+3V3/+9V_OPAMP/VREF와 무입력 noise 확인
   - 100mVrms 1kHz LINE/INST, clip, 전원 ON/OFF Thru 기준 기록
14. **SD 실기 브링업**
   - 완료된 `SZH-EKBZ-005` 배선과 `+5V`↔`+3V3` 미연결을 무전원 검사
   - FAT32 `GG/images` mount·탐색, LCD 공유 SPI, 입력 응답과 전원 OFF 카드 교체 확인
15. KiCad 커스텀 풋프린트 배치와 실측: DevKit 행간, 6.35mm 잭, MP1584, 9V 커넥터
16. 튜너 5분 무리셋·I2S overflow와 실제 오디오 onset/Curve/Reference 실기 튜닝
17. `CONFIG_LV_DEF_REFR_PERIOD=33ms`에서 15ms 변경 여부 결정
18. 미래 코덱 모듈 선정: PCM5102A와 ES8388 중 선택
19. OPA2192 자동 듀얼레인지 부품 조달·별도 실장·1kHz/sweep 교정
20. GG Analog Meter USB HID·바늘 ballistics·전력 예산 설계
21. 16MB 앱 파티션 확장과 NVS 보존·플래시 마이그레이션 계획

## 해결 및 관찰

- input WDT: 10ms 폴, 최소 1 tick, input prio3 적용 후 310초 무재발
- 정상 UI와 실제 FOOTSW 짧게 전환 확인
- TRS 6키 전부 판정창 통과, UP/DOWN 실측 간격 110mV
- HOME 롱은 Launcher, FOOTSW 롱은 Tuner 전환 확인
- 결정론적 simulator smoke CLI 2/2 통과, A#3 233.59Hz 잠금과 NVS 비변경 확인
- USB-only 오디오에서는 TL072가 꺼져 PCM1808 입력이 부유하므로 Tuner 판정을 신뢰하지 않음
- I2S 폭주 대응: DMA 8, 튜너 HOP 384, 로그 2초 제한, 64블록 yield 반영
- 저장소 위생: `build/`, `managed_components/`, 파생 `sdkconfig*` 추적 제거
- 사용자 확인: SD 카드 모듈 배선 완료. 기능 브링업은 대기
- 오디오 입력 구간은 TL072+SPDT 기준으로 재배선 필요. OPA2192는 미보유
- Ring 100Ω / Tip 220Ω은 현재 장착됨
- SD 모듈 정정: `SZH-EKBZ-005`는 4.5~5.5V 급전형이므로 VCC=`+5V`; SPI 신호는
  온보드 레벨 변환을 거치는 3.3V 로직
- NVS v1 -> v2 최초 부팅 시 1회 기본값 리셋은 정상
- 3행 런처: 빈 STASH 경유, Settings/Reorder 왕복, 선택 테두리와 대각 커서 실기 통과
- Sound Monitor: 20Hz~20kHz 로그 축, -72~0dBFS 무가중 분석,
  선택형 A-weighted 표시. Curve는 평활 현재선·채움, Reference는 무평활·무잔상이며 peak
  hold는 12-Band에만 표시. PC 시각 검수와 COM4 정상 부팅 통과
- Sound Monitor 12밴드:
  50/100/200/400/600/800/1.2k/1.6k/3.2k/4.5k/6.4k/10kHz 적용
- 얼굴형 렌더러 제거, 고정 중심·PSRAM 기반 72-segment Circular spectrum으로 교체
- dB Meter 앱: LIVE 블록 또는 최근 1초/3초 전체 표본의 RMS 전력 평균, 5Hz 표시,
  sample peak dBFS와 1초 hold, 수동 LINE/INST 이득 기반 입력 잭 명목 Vrms·dBV·dBu 구현
- Bounce 앱: SPECTRUM 온셋 기반 고양이·종이컵 러너, 점수·충돌·온셋 재시작 구현
- 앱 외형 소유권 분리: 전역 UI Theme은 런처·모든 공통 팝업, 앱 Color는 해당 앱만 변경.
  전역 Theme은 `Dark/Light × Blue/Green/Yellow/Red`, 모든 앱은
  `Settings → Theme → Mode/Color`에서 `Default/Blue/Green/Yellow/Red` Color와 Mode를
  제공하며 Sound Monitor는
  4개 Mode, Bounce는 Classic Cat만 제공. NVS v6는 v2~v5 blob을 보존 마이그레이션
- 사용자 실기: 전역 Theme과 앱 팝업 팔레트 연동은 정상. Nyan은 장애물 등장 시
  프레임·입력 지연이 심해 제거
- `f2182fea`를 외부 9V 분리·USB 단독 상태에서 COM4에 플래시. 약 25초 부팅 로그에서
  8MB PSRAM 80MHz, 240MHz, ST7796/LVGL, ladder IDLE과 오류 0회 확인
- 사용자 실기: 공통 Color/Mode 화면과 Classic Cat 장애물 구간의 짧은
  HOME/FOOTSW 응답을 포함해 이상 없음
- 초기 Sound Monitor 개발본은 Curve 상하 기울기와 좌우 단순화를 제공했다. 이후 정확한
  모니터링 계약에 따라 기울기는 제거하고 좌우 `DETAIL/BALANCED/SIMPLE`만 유지했다.
- Curve/Reference 개발본: ESP-IDF 기본 `0xd2040`·래더 비활성 `0xce9c0`,
  호스트 4/4, COM4 일반 플래시와 25초 무오류 부팅 로그 통과
- Basic 래더는 판정창 변경 없이 6키 UI 동작이 다시 정상임을 사용자가 확인했다.
  전압 재수집은 하지 않았으며 앞선 이탈은 일시적 접점 변화로 관찰한다.
- 사용자 실기에서 당시 Curve tilt·단순화 조작과 Reference `FLAT`, 짧은 HOME/FOOTSW가
  모두 정상 동작했다. tilt 제거·저역 다중 해상도 개발본은 아직 본체에 플래시하지 않았다.
  USB-only 무입력의 20~70Hz 약 -65dBFS 성분은 TL072 무전원
  부유 입력의 60Hz 주변 험으로 분류하며 외부 9V 실오디오 시험 전에는 성능 판정에서 제외한다.
- PC 시뮬레이터의 별도 256-point DFT를 제거하고 본체와 같은 `fft_map.c`를 직접 빌드한다.
  23.4375Hz bin, 로그 매핑, 65ms 평균, 220ms release와 peak hold가 양쪽에서 일치하며
  46.875Hz 저역·peak 지속 회귀 테스트를 추가했다.
- Windows 루프백이 재생 중단 뒤 무음 패킷을 보내지 않는 구간에는 실시간 48kHz 무음 표본을
  보충해 release가 멈추지 않도록 했다. Curve/Reference의 peak 선을 제거하고 12-Band
  마커만 유지했으며, 5초 무음 뒤 현재선·peak 바닥 복귀 회귀 테스트가 통과했다.
- Sound Monitor 1차 PC 최적화로 Curve/Reference의 숨은 peak 계산·상태 추적을 제거하고
  목표 주기를 30ms로 조정했다. 결정론적 smoke에서 약 28fps가 31~32fps로 개선됐고,
  Gallery의 어두운 `GG` 빈 상태도 자동 회귀 검사에 포함했다.
- 사용자 sweep 캡처에서 41Hz가 20~50Hz의 넓은 평면으로 복제되는 현상을 확인했다.
  `dB/oct` 경로를 제거하고 12kHz 저역 FFT를 추가한 뒤 41Hz peak=40.9Hz/-12dB 폭=19.5Hz,
  108Hz=105.1Hz/14.4Hz, 1037Hz=1041.9Hz/83.2Hz 회귀 검사가 통과했다.
- 후속 sweep 캡처의 저역 폭·오른쪽 급경사와 500Hz 이중 피크를 분석해 Reference 전용
  3kHz/2048 FFT, 2Hz DC blocker와 3개 창 중심 정렬을 추가했다. 고정 진폭 30/300/3000Hz는
  peak `-7.39/-6.26/-6.02dBFS`, +16.7% 지점 `-43.22/-72/-72dBFS`였고,
  250~700Hz sweep 76프레임에서 주요 이중 피크가 없었다.
