# AGENTS.md - GG 작업 규칙

이 파일은 이 저장소에서 작업하는 에이전트가 매 세션 자동으로 따를 지속 규칙이다.
작업 언어와 문서 언어는 한국어다.

## 1. 세션 시작 순서

1. `git status --short`와 최근 커밋을 확인한다. 사용자의 기존 변경을 되돌리지 않는다.
2. `LAB_STATE.md`에서 마지막 플래시와 현재 차단 요인을 확인한다.
3. `PUNCHLIST.md`와 `PROJECT_MASTER.md`에서 현재 우선순위를 확인한다.
4. 작업 영역의 SSOT와 실제 코드를 함께 읽는다. 추측으로 지시서나 코드를 작성하지 않는다.
5. 한글 문서는 UTF-8이다. Windows PowerShell 5.1에서는 `Get-Content -Encoding utf8`을 사용한다.

`CLAUDE_HANDOFF.md`는 2026-07-26 시점의 인수인계 스냅샷이다. 현재 상태의 권위는
`LAB_STATE.md`와 실제 코드에 있으며, 충돌하면 실기 로그 > 코드 > 현재 SSOT > 인수인계
스냅샷 순으로 판단하고 문서를 즉시 바로잡는다.

## 2. 문서 권위

| 영역 | SSOT |
|---|---|
| 현재 장치·플래시·실기 상태 | `LAB_STATE.md` |
| 미결 작업과 우선순위 | `PUNCHLIST.md` |
| 총괄 로드맵과 확장 트랙 | `PROJECT_MASTER.md` |
| GG 제품 정체성·범위·GG2 경계 | `GG_PRODUCT_SPEC.md` |
| 펌웨어 구조와 앱 계약 | `ARCHITECTURE.md` |
| 하드웨어 넷·핀·저항 | `hardware/NETLIST_SPEC.md` |
| 브레드보드 조립 절차 | `ASSEMBLY.md` |
| Basic/Smart 컨트롤러 계약 | `CONTROLLER_DESIGN.md` |
| 런처와 UI | `LAUNCHER_DESIGN.md`, `UI_DESIGN.md` |

코드 변경이 문서화된 계약이나 실기 상태를 바꾸면 같은 작업에서 관련 SSOT와
`LAB_STATE.md`를 갱신한다.

## 3. 작업 흐름

- 태윤은 제품 방향과 물리 하드웨어 판단을 맡고, Codex는 요구사항 정리, 설계, 구현,
  자동 검증, diff 리뷰, 플래시와 로그 분석을 한 흐름으로 수행한다.
- 복잡하거나 위험한 변경은 구현 전에 계획과 불변조건을 사용자와 합의한다.
- `CODEX_INSTRUCTION_*.md`는 복잡한 작업의 일회용 실행 명세다. 적용과 검증이 끝나면
  Git 이력만 남기고 삭제한다. 지속 규칙을 일회용 지시서에만 남기지 않는다.
- 변경은 목적별로 작게 유지한다. 무관한 리팩터링과 사용자 변경의 복구를 하지 않는다.
- 커밋은 검증 후 목적별로 만들고, 사용자가 요청하지 않으면 push하지 않는다.

## 4. 펌웨어 불변조건

- Core1만 I2S와 DSP 상태를 변경한다. UI는 발행된 스냅샷만 읽는다.
- 앱은 `gadget_app_t` 레지스트리와 활성 앱 디스패치 계약을 유지한다.
- 모든 LVGL 접근은 기존 lock 규약을 지킨다.
- `input_task`는 물리 입력의 샘플링·판정과 UI 큐 적재만 맡는다. LVGL 디스패치나 NVS
  쓰기를 동기 호출해 입력 폴링을 막지 않는다.
- `input_button_read_raw()`는 Basic/Smart 입력 전환 경계이므로 캡슐화를 유지한다.
- 래더 판정은 비율 기반 window+deadzone 방식이다. 최근접 판정은 사용하지 않는다.
- 래더 Rtop은 10k이고 Ring은 +3V3 고정이다. 5V 인가는 금지한다.
- `INPUT_HOLD_MS`, `INPUT_REPEAT_DELAY_MS`, `INPUT_REPEAT_RATE_MS`는 사용자 체감 계약이다.
  변경하려면 실시간 단위가 유지되는지 계산하고 실기로 확인한다.
- 태스크 주기나 우선순위를 바꾸기 전에 코어 배치, 우선순위, 블로킹 구간,
  `CONFIG_FREERTOS_HZ`와 tick 반올림을 계산한다. `vTaskDelay(0)` 가능성을 허용하지 않는다.

## 5. 자동 검증

펌웨어 변경의 기본 완료 조건:

1. 깨끗한 ESP-IDF 구성에서 `sdkconfig.defaults`를 사용한 전체 빌드와 `-Werror` 통과
2. `INPUT_TRS_LADDER=0` 컴파일 통과
3. `tests/`에 등록된 호스트 테스트 전부 통과
4. 앱·UI·공유 DSP 변경 시 PC 시뮬레이터의 깨끗한 빌드와 `--smoke-test` 통과
5. 앱·UI·공유 DSP 변경 시 추적 중인 `sim/build/pedal_sim.exe`를 같은 커밋에서 갱신
6. `git diff --check`와 최종 diff 리뷰

`build/`, `managed_components/`, `sdkconfig`, `sdkconfig.old`는 파생 로컬 상태이며 Git에
추적하지 않는다. 오래된 `build/` 캐시를 신뢰하지 말고 필요하면 깨끗한 임시 빌드를 사용한다.

본체와 PC 시뮬레이터는 앱·UI·renderer·`fft_map`을 같은 소스에서 빌드한다. 시뮬레이터는
입력 수집과 FFT 실행 같은 플랫폼 백엔드만 별도로 구현하며 스펙트럼 매핑·시간 평활·release·
peak hold를 재구현하지 않는다.

## 6. 하드웨어 검증 안전 규칙

- USB 플래시 전에 태윤에게 외부 9V가 분리됐는지 확인한다. USB와 외부 9V를 동시에
  연결하지 않는다.
- 다른 터미널이 COM 포트를 점유하지 않았는지 확인한다.
- 플래시 전체 삭제, 파티션 변경, 보안 설정, eFuse 작업은 별도 명시와 승인이 없으면 하지 않는다.
- 뮤트 J201 게이트에 G3를 직결하지 않는다.
- TRS Ring에 5V를 연결하지 않는다.
- 실기 검증 후 커밋, 포트, 관찰 시간, 로그 결과, 통과/실패 항목을 `LAB_STATE.md`에 기록한다.
- 화면·소리·버튼 조작처럼 로그만으로 판정할 수 없는 결과는 태윤의 관찰을 사실로 기록한다.
