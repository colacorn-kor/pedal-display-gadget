# GG PC 시뮬레이터

PC 시뮬레이터는 GG와 같은 앱·UI·renderer·FFT·튜너 코드를 LVGL 9.5와 SDL2 위에서
실행한다. 단순 화면 미리보기가 아니라 새 앱과 설정을 먼저 완성하는 기준 구현이며,
GG 없이도 사용할 수 있는 독립 데스크톱 앱으로 개발한다. 제품 계약과 이식 순서는
`../PC_SIMULATOR_PRODUCT.md`가 권위다.

현재 Windows 시스템 오디오/캡처 입력, PC 폴더 Gallery, 기존 5개 앱은 동작한다.
Music은 파일 목록만 있고 재생은 없으며, Game도 게임 목록만 있고 실행 코어는 없다.

## 빌드

```powershell
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
.\sim\build\pedal_sim.exe
```

SDL2가 표준 위치에 없으면 `-DSDL2_DIR=...`를 전달하거나 SDL2 패키지 경로를
`CMAKE_PREFIX_PATH`에 지정한다. Windows에서는 Visual Studio Build Tools 등 호스트 C/C++
컴파일러도 `PATH`에 있어야 한다.

## 입력

- 방향키: `EV_UP`, `EV_DOWN`, `EV_LEFT`, `EV_RIGHT`
- Enter: `EV_OK`
- Backspace: `EV_HOME`, 500ms 유지하면 `EV_HOME_HOLD`
- Space: `EV_FOOTSW`, 500ms 유지하면 `EV_FOOTSW_HOLD`
- `O`: 오디오 캡처 없이 합성 onset 주입
- 마우스 X 위치: 오디오 캡처 없이 E2~E5 합성 피치
- Escape: 종료

## 오디오 입력

Windows 기본값은 WASAPI loopback이다. PC의 기본 스피커나 헤드폰으로 재생되는 소리를
48kHz mono 분석 입력으로 변환해 공통 Sound Monitor, Tuner, dB Meter와 music event
DSP에 보낸다. 기본 경로를 명시하려면 다음과 같이 실행한다.

```powershell
.\sim\build\pedal_sim.exe --system-audio
```

기본 마이크 또는 오디오 인터페이스 캡처 입력을 사용하려면:

```powershell
.\sim\build\pedal_sim.exe --microphone
```

사용 가능한 시스템 입력과 SDL2 캡처 장치를 확인하거나 번호로 고르려면:

```powershell
.\sim\build\pedal_sim.exe --list-audio
.\sim\build\pedal_sim.exe --audio-device 1
```

Windows가 완전한 무음에서 loopback 패킷을 보내지 않으면 시뮬레이터가 경과 시간에 맞는
0 표본을 보충한다. 따라서 공통 FFT의 평균·release·peak hold가 정지하지 않고 표시 하한까지
내려간다.

시뮬레이터는 펌웨어의 `fft_map.c`를 직접 빌드한다. 48kHz/2048-point 23.4375Hz,
12kHz/2048-point 5.859375Hz, 3kHz/2048-point 1.46484375Hz 분석과 창 중심 정렬,
로그 매핑, 평균, release와 peak hold는 본체와 같다. Reference는 시간 평균과 release 없이
세 해상도를 혼합한다. 오디오 수집과 FFT 실행 백엔드만 WASAPI/SDL·portable C와
I2S·ESP-DSP로 나뉜다. 분석값에는 `dB/oct` 기울기를 적용하지 않으며 Curve의 평활과
선택형 `Flat/A-weighted/Flat(Loudness)/A-weighted(Loudness)` 응답은 렌더러에 전달되는
복사본만 바꾼다. Loudness는 1kHz 기준 60-phon 비교 표시이며 절대 청취 loudness가 아니다.
Sound Monitor 화면에서는 상·하로 Weighting을 바로 변경한다. 표시 하한 부근의 양의
보정은 부드럽게 줄어 무음 release 잔여값을 신호처럼 끌어올리지 않는다.
Curve와 Reference 배경은 Sub Bass/Bass/Low-Mid/Mid/High-Mid/High 여섯 구간을 옅은
색으로 나누고 `Bass/Mid/High`만 글자로 표시한다.

캡처 장치를 열 수 없을 때는 경고를 출력하고 합성 visualizer, 마우스 pitch와 `O` onset을
사용한다. 캡처 없이 결정론적 화면을 보려면:

```powershell
.\sim\build\pedal_sim.exe --synthetic
```

## 화면 미리보기

NVS 상태와 키 입력 없이 특정 화면을 연다.

```powershell
.\sim\build\pedal_sim.exe --preview curve
.\sim\build\pedal_sim.exe --preview reference
.\sim\build\pedal_sim.exe --preview bars
.\sim\build\pedal_sim.exe --preview circular
.\sim\build\pedal_sim.exe --preview gallery
.\sim\build\pedal_sim.exe --preview dbmeter
.\sim\build\pedal_sim.exe --preview bounce
.\sim\build\pedal_sim.exe --preview monitor-menu
.\sim\build\pedal_sim.exe --preview monitor-settings
.\sim\build\pedal_sim.exe --preview monitor-color
.\sim\build\pedal_sim.exe --preview monitor-mode
.\sim\build\pedal_sim.exe --preview monitor-weighting
```

런처 Theme은 `Mode=Dark/Light`와 `Color=Blue/Green/Yellow/Red`를 독립적으로 저장한다.
각 앱 Color는 `Default/Blue/Green/Yellow/Red`이며 Default는 전역 조합을 상속한다.
런처 Settings는 `Theme/About`, 앱 홈 메뉴는 `Settings/Info`다. 앱 Settings의 Theme
아래에 앱별 `Mode/Color`가 있고 Sound Monitor는 Theme 뒤에 Weighting을 둔다.
dB Meter 화면은 좌·우로 `INPUT/WINDOW`를 선택하고 상·하로 값을 바꾸며, 같은 설정은
`Settings → Input/Window`에도 있다.
런처·테마·슬롯의 지속 상태는 현재 작업 디렉터리의 `sim_nvs.bin`에 저장된다.

## PC SD 폴더

기본적으로 다음 폴더를 GG의 MicroSD처럼 사용한다.

```text
sim/sdcard/GG/images
sim/sdcard/GG/music
sim/sdcard/GG/games
```

다른 폴더를 쓰려면 `GG_SD_ROOT` 환경변수를 지정한다. Gallery는 `GG/images`를 실제로
표시한다. 이미지가 없으면 어두운 `GG` 월페이퍼를 표시한다. Music과 Game은 현재
필터·정렬·안전한 상대경로 카탈로그만 공유하며 재생·실행은
후속 구현이다.

```powershell
.\sim\build\pedal_sim.exe --preview gallery
```

## 결정론적 Smoke Test

키보드 자동화 없이 런처, 앱 디스패치, 합성 visualizer, 공통 tuner DSP, Bounce 점프·충돌·
재시작, 퀵 앱과 앱 정리를 검사한다.

```powershell
.\sim\build\pedal_sim.exe --smoke-test
```

이 모드는 합성 오디오와 기본 런처 상태를 사용하고 `sim_nvs.bin`을 읽거나 쓰지 않는다.
모든 검사를 통과하면 종료 코드 0과 `SMOKE PASS`를 출력한다.
