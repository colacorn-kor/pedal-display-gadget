# PUNCHLIST.md - 미결 및 확인 항목

> 현재 장치 상태와 실기 절차는 `LAB_STATE.md`가 권위다.

## P0 - 현재 차단 요인

- 없음

## P1 - PC 기준 제품

1. **D0A Sound Monitor 우선 최적화**
   - 완료: Curve/Reference Slope 0, 중심 정렬 3단계 FFT, DC 차단, Reference 무잔상,
     30/300/3000Hz·경계 sweep 회귀 검사, Flat/A-weighted와 60-phon Loudness 표시,
     상하 직접 전환과 Weighting 무음 바닥 보존
   - 완료: 12-Band·Circular 프레임 시간과 HOME 응답 계측, 계산·그리기 병목 축소,
     Circular 20kHz 위/20Hz 아래 배치와 픽셀 단위 좌우 대칭 회귀 검사
   - 완료: Curve/Reference OK 비교 기준선, 튜너 무음 gate·적응 평활·2프레임 음이름 확인,
     dB Meter BS.1770 K-weighted M/S/I LUFS와 4x true-peak 추정·리셋
2. **D0 기존 UI·설정 기준선 감사**
   - Launcher, Sound Monitor, Gallery, Tuner, dB Meter, Game의 모든 화면·빈 상태·오류 상태 목록화
   - 전역 Dark/Light·Color와 앱 Color/Mode·세부 설정의 실제 값·표시·NVS 복원 일치 확인
   - 앱별 preview와 smoke 수용 기준 확장
3. **D1 플랫폼 능력과 PC 오디오 출력 기반 - 완료**
   - 완료: `needs_codec`를 앱 요구 capability와 플랫폼 `AUDIO_PLAYBACK_OUTPUT` 검사로 교체
   - 완료: 단일 앱 소유권, Music/Effects 버스, gain·클리핑을 갖춘 공통 스테레오
     transport/mixer와 PC SDL queued output 구현
   - 완료: 코덱 없는 ESP는 명시적 unavailable이며 큐를 할당하지 않고 같은 공통 소스를 빌드
4. **D2 Music 앱 - 완료**
   - 완료: `GG/music` 탐색, WAV 재생/일시정지, 이전/다음, 진행률, 볼륨, 오류 상태
   - 완료: 파일이 없을 때 코드 생성 8비트 로비 음악 재생, 앱 종료 시 정지·소유권 해제
   - MP3/FLAC/OGG는 메모리·라이선스·ESP 이식성 확인 뒤 공통 디코더 경계에 추가
5. **D3 Game 타일 로비와 내장 게임 - 완료**
   - 완료: 4칸 정사각형 로비, 좌우 선택, 빈 타일의 메타데이터 없는 GG Cat 이스터 에그
   - 완료: Chrome Dino식 대기·가속·선인장·충돌 흐름, 버튼/오디오 임계치 점프,
     내장 게임 HOME=선택 위치를 보존한 로비 복귀
   - 완료: 외부 코어가 검사한 실행 가능 ROM만 빈 타일 앞에 표시
6. **D4 Metronome과 앱 효과음 - 완료**
   - 완료: 40~220 BPM, 2~5박자, 4종 subdivision, 첫 박 accent와 sample-clock click
   - 완료: OK 재생/정지, 좌우 항목 선택, 상하 값 변경, 앱 Settings와 지연 NVS 저장
   - 완료: Game 내장 GG Cat의 점프·통과·충돌 효과음을 공통 Effects 버스에 연결
   - 현재 GG는 코덱이 없어 Metronome을 무음 시각 모드로 실행하고, PC는 실제 출력 사용
7. **D5 Gallery 미디어 UX - 완료**
   - 완료: Scanning/Loading/Ready/Empty/Error 상태와 어두운 `GG` 폴백
   - 완료: 대소문자 무시 자연 정렬, 손상 파일 검사, 한 줄 긴 파일명, 메타데이터 표시
   - 완료: OK 새로고침 뒤 선택 파일 보존, 5초 무입력 정보 배너 숨김·입력 복귀,
     PC/SD 공통 탐색 결과와 오류 문구
8. **D6 외부 Game 코어 - 완료**
   - 완료: MIT Peanut-GB 기반 DMG `.gb` 코어, 공통 프레임·입력·오디오 sink·save RAM 어댑터
   - 완료: 헤더/체크섬/크기/카트리지 검사 통과 파일만 타일 노출, 2x 화면과 `.sav` 저장
   - 완료: PC와 GG가 같은 코어·ROM·입력 어댑터를 빌드하며 기존 1MiB 파티션 유지
   - 외부 Game Boy 오디오는 아직 무음이며 GBC 전용/GBA/NDS는 지원하지 않음
9. **D7 MIDI Monitor - 완료**
   - 완료: 공통 MIDI 이력·Program Change 서비스와 앱별 capture 모드
   - 완료: Windows WinMM MIDI 입출력·장치 열거와 CLI 선택
   - 완료: 채널 필터·pause·clear·clock 집계를 갖춘 MIDI Monitor
   - GG의 TRS-A 물리 회로는 확정됐고 부품 조달·조립과 UART/BLE-MIDI 전송 백엔드가 남음

## P2 - PC 앱 완성도와 지속 검증

10. **시스템 오디오·마이크·미디어 폴더·출력·MIDI 장치 선택 UI - 현재**
11. 창 배율·전체화면·게임패드 입력과 배포 가능한 실행 패키지 정리
12. 모든 새 공통 기능에 preview/smoke/호스트 테스트와 ESP 기본·래더 비활성 빌드 유지
13. 삭제된 Python 3.14 경로를 가리키는 ESP-IDF v5.4.4 공식 Python 환경 복구

## P3 - 병행 하드웨어

14. **남은 하드웨어 부품 조달과 목표 배선 - 현재**
   - 완료: TLV320DAC3100 재생/AUX/헤드폰 모듈, TRS-A MIDI 회로,
     G39 AQY221R2S 병렬 뮤트 선정과 핀 단위 문서화
   - `hardware/PURCHASE_LIST.md` 부품 구매 뒤 `ASSEMBLY.md` 순서로 무전원 배선 대조
   - 임시 TL072+LINE/INST를 다시 만들지 않고 OPA2192 자동 듀얼레인지로 진행
   - 현재 오디오 끝점을 직접 확인해 `hardware/AS_BUILT_WIRING.md`의 미확인 행 갱신
   - `ASSEMBLY.md` 권장안 반영 뒤 As-Built를 다시 현재 상태로 갱신
   - 전원 OFF에서 Thru 연속성·레일 단락 확인
   - USB 분리·외부 9V 단독으로 +5V/+3V3/+9V_OPAMP/VREF와 무입력 noise 확인
   - 100mVrms 1kHz HOT/SENSITIVE, clip, 전원 ON/OFF Thru 기준 기록
15. **SD 실기 브링업**
   - 완료된 `SZH-EKBZ-005` 배선과 `+5V`↔`+3V3` 미연결을 무전원 검사
   - FAT32 `GG/images` mount·탐색, LCD 공유 SPI, 입력 응답과 전원 OFF 카드 교체 확인
16. KiCad 커스텀 풋프린트 배치와 실측: DevKit 행간, 6.35mm 잭, MP1584, 9V 커넥터
17. 튜너 5분 무리셋·I2S overflow와 실제 오디오 onset/Curve/Reference 실기 튜닝
18. `CONFIG_LV_DEF_REFR_PERIOD=33ms`에서 15ms 변경 여부 결정
19. **ESP 재생/MIDI 하드웨어 backend**
   - TLV320DAC3100 I2C 초기화, I2S0 RX/TX, Core1 playback 소비와 capability 연결
   - UART MIDI IN/OUT을 기존 `midi_feed()`/service에 연결
20. OPA2192 자동 듀얼레인지 부품 조달·별도 실장·1kHz/sweep 교정
21. GG Analog Meter USB HID·바늘 ballistics·전력 예산 설계
22. **16MB 앱 파티션 확장 - 자동 검증 완료, 실기 마이그레이션 대기**
   - 완료: NVS·PHY·factory 주소를 유지한 factory 4MB + OTA 4MB 2슬롯 표와 기본 설정
   - 완료: ESP-IDF 파티션 검증, 기본·래더 비활성 `-Werror=all` 빌드, 호스트 17/17
   - 대기: COM 포트 재연결 뒤 NVS 24KB 백업, 전체 삭제 없는 일반 플래시, 부팅·설정 보존 확인

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
- 오디오 입력 구간은 OPA2192 자동 듀얼레인지로 재배선 예정. OPA2192는 미보유
- 코덱은 TLV320DAC3100, MIDI는 6N138+SN74AHCT14, 뮤트는 G39+AQY221R2S로 확정
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
  sample peak dBFS와 1초 hold, 수동 LINE/INST 이득 기반 입력 잭 명목 Vrms·dBV·dBu 구현.
  좌우 Input/Window 선택·상하 값 변경과 `Settings → Input/Window` 공통 상태 연결 완료
- 독립 Bounce 앱 제거. Game의 빈 타일에서만 이름 없이 내장 GG Cat을 실행하며,
  고양이·선인장 Dino 흐름과 버튼/오디오 레벨 점프를 공통 런타임으로 구현
- 앱 외형 소유권 분리: 전역 UI Theme은 런처·모든 공통 팝업, 앱 Color는 해당 앱만 변경.
  전역 Theme은 `Dark/Light × Blue/Green/Yellow/Red`, 모든 앱은
  `Settings → Theme → Mode/Color`에서 `Default/Blue/Green/Yellow/Red` Color와 Mode를
  제공하며 Sound Monitor는 4개 Mode를 제공. NVS v6는 v2~v5 blob을 보존 마이그레이션
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
