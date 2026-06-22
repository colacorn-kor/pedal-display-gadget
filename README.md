# 기타 페달보드 미니 디스플레이 가젯 — 펌웨어

손바닥 크기 기타 페달보드용 디스플레이 가젯. **튜너 + 사운드 시각화 + 이미지/콘텐츠 표시 + 출력 뮤트**가 기본 기능이고, MIDI는 옵션 확장(리그 통합)이다. "MIDI 컨트롤러가 아니라, MIDI로 똑똑해지는 튜너 겸 비주얼 디스플레이"가 컨셉.

## 타깃 / 툴체인
- **MCU**: ESP32-S3-WROOM-1 N16R8 (16MB flash / 8MB PSRAM) — 프로토타입은 ESP32-S3-DevKitC-1
- **디스플레이**: ST7796 3.5" 480×320 SPI (4선)
- **오디오 ADC**: PCM1808 I2S (24-bit, 48kHz). 인라인 아날로그 버퍼에서 병렬 탭(통과 톤에 영향 없음)
- **프레임워크**: ESP-IDF v5.x, LVGL v9
- **컴포넌트**: `lvgl/lvgl^9`, `espressif/esp_lvgl_port^2.6`, `espressif/esp_lcd_st7796`, `espressif/esp-dsp`

## 아키텍처 (데이터 흐름)
```
오디오 IN → I2S(Core1 단독) ── audio_task
   ├─ AUDIO_SPECTRUM: fft_feed() → sequence 보호 스냅샷(bars+peaks+level)
   └─ AUDIO_TUNER   : anti-alias FIR → MPM/NSDF → sequence 보호 결과
        ↓ (release/acquire 발행 + 일관된 복사)
Core0 display_task: lvgl_port_lock → sm_render() → 활성 렌더러/튜너 화면 갱신 → unlock
Core0 input_task : 버튼/풋스위치/UI queue → lvgl_port_lock → sm_on_event() → unlock
(Phase 2) MIDI UART RX → midi_feed() → midi_on_message() → UI queue (LVGL 직접 접근 없음)
```

### 핵심 설계 원칙
1. **오디오 무지연**: 오디오 파이프라인과 DSP 상태를 Core1이 단독 소유. 디스플레이/입력이 오디오를 블록하지 않음. 슬롯별 sequence counter와 release/acquire 발행으로 UI는 일관된 프레임만 복사.
2. **분석 ↔ 렌더 분리**: 오디오는 `viz_frame_t`(스펙트럼 256점 + level)만 만들고, 등록형 `renderer_t`(곡선/막대/반응형)가 그림. 테마 = vtable 구현 + 팔레트. 다운로드 테마의 토대.
3. **무음 튜닝**: 청취 탭과 통과 경로가 독립 → 출력을 뮤트해도 튜너/시각화는 계속 동작. 풋스위치 = 어디서나 튜너+뮤트 토글.
4. **MIDI = 리그 통합**: Program Change → 씬(콘텐츠+테마+렌더러) 자동 전환. 옵토 절연이라 그라운드 루프도 없음.

## 파일 맵
| 파일 | 역할 |
|------|------|
| `audio_config.h` | 샘플레이트·시각화 포인트 수 공용 불변식 |
| `app.h` | 모듈 간 공유 선언: 오디오 스냅샷, UI queue, 뮤트, 화면 매니저 API |
| `app_main.c` | 부팅 + 3태스크(audio Core1 / display·input Core0) + sequence 스냅샷·UI queue·I2S |
| `fft_map.{c,h}` | 스펙트럼 매핑: 2048-pt FFT(esp-dsp) → 256점 로그 곡선(20Hz–20kHz), 틸트·평활·피크홀드, Monitor/Visualizer 프리셋 |
| `tuner.{c,h}` | 시간영역 피치검출(MPM/NSDF), 48k→12k 데시메이션, 음이름+센트 |
| `renderer.{c,h}` | 렌더러 레지스트리 + 내장 테마(Classic/Robot/Cute) |
| `renderer_curve.c` | 곡선 렌더러(PSRAM 캔버스 직접 픽셀) |
| `renderer_bars.c` | 막대 렌더러(256점→32막대 그룹핑) |
| `renderer_reactive.c` | 반응형 캐릭터 렌더러(level/centroid/onset → 몸·눈·입·색) |
| `content_screen.{c,h}` | 이미지/GIF/텍스트 표시 + SD 파일시스템 브리지(`S:` → `/sdcard`) |
| `tuner_screen.{c,h}` | 튜너 화면(음이름 + 센트 니들 + 인튠 존) |
| `screen_manager.c` | 화면 상태머신(HOME/MONITOR/IMAGES/TUNER) + 렌더러/테마 순환 + 뮤트 + 씬 로딩 |
| `display_bringup.{c,h}` | ST7796 + esp_lvgl_port 초기화(`bsp_display_init`) + 스모크 테스트 |
| `midi.{c,h}` | MIDI 1.0 바이트 파서(러닝 스테이터스/리얼타임) |
| `midi_map.c` | MIDI 이벤트 → 동작 매핑(PC→씬, CC→동작, Clock→BPM) |
| `*_mockup.svg` | 화면 디자인 목업(코드 아님, 참조용) |

## 빌드 / 설정
```bash
idf.py add-dependency "lvgl/lvgl^9.5.0"
idf.py add-dependency "espressif/esp_lvgl_port^2.6"
idf.py add-dependency "espressif/esp_lcd_st7796^1.4.0"
idf.py add-dependency "espressif/esp-dsp^1.8.2"
```
저장소의 `main/idf_component.yml`과 `sdkconfig.defaults`에도 동일한 의존성과 기본 설정이 포함되어 있다.

`menuconfig` → LVGL: Color depth **16(RGB565)**, 폰트 **Montserrat 12·14·28·48**, **LV_USE_CANVAS**, **LV_USE_GIF** + 필요한 이미지 디코더.

## 불변식 / 결합 (검수 시 확인)
- `VIZ_POINTS`는 `audio_config.h` 한 곳에서만 정의
- MIDI 씬은 숫자 인덱스 대신 렌더러 이름(`curve`, `bars`, `reactive`)으로 해석
- 디스플레이/입력 태스크의 **모든 LVGL 호출은 `lvgl_port_lock/unlock`로 보호** (esp_lvgl_port가 LVGL 태스크를 자체 구동하므로 `lv_timer_handler` 직접 호출 안 함)
- Core1만 DSP 상태를 변경하며, UI는 `audio_viz_snapshot_get()`/`tuner_get()`의 일관된 복사본만 사용
- `content_fs_register()`는 `lvgl_port_init()` 이후 LVGL lock 안에서 한 번만 호출

## 의도된 스텁 / Phase 2 (버그 아님)
- **핀 번호**(I2S/디스플레이/버튼/뮤트)는 플레이스홀더 — 실제 배선에 맞춰 조정
- **SD 마운트**(`esp_vfs_fat_sdspi_mount`) 미구현 — `content_fs_register`는 `/sdcard` 마운트 가정
- **MIDI**: 파서+매핑 완료, UART RX/TX + 옵토 절연 회로는 Phase 2 (app_main에 미연결)
- **BLE**: 미구현 (Phase 2)
- **테마/씬/이미지 테이블**: 하드코딩 플레이스홀더 — 추후 SD manifest/JSON
- **mute_set**: GPIO 레벨만 — 소프트 램프는 하드웨어 RC가 담당
- **입력**: 폴링 디바운스 — 길게누름/ISR은 추후
- **반응형 렌더러**: centroid/onset를 렌더러 내부에서 계산(파이프라인 비변경)

## 검증
ESP-IDF 빌드:
```bash
idf.py set-target esp32s3
idf.py build
```

호스트 MIDI/튜너 테스트:
```bash
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

실기에서는 이미지 화면 반복 진입/이탈, MIDI burst 중 화면 전환, I2S overflow count, PSRAM 부족 경로를 추가로 스트레스 테스트한다.
