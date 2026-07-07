# Codex 지시서(소형 2건) — bars/reactive 최적화 · TRS ADC 입력 리워크

## 건① bars/reactive 렌더러 최적화 (curve 패턴의 오브젝트판)
- 근거: curve 최적화 시 Codex 자체 메모 — 이 둘은 캔버스가 아닌 LVGL 오브젝트 기반.
- `renderer_bars.c`: bar 32개별 **이전 높이/색/피크y 보관** → 변화 없으면 `lv_obj_set_*`/
  `lv_obj_set_style_*` 호출 자체를 건너뜀(LVGL 내부 invalidate 억제). 피크 라인 동일.
- `renderer_reactive.c`: 이전 geometry/color 비교 후 변경분만 set. 
- 불변조건: renderer_t 인터페이스·이름 유지, 오디오 무변경, -Werror. 
- 판정: 정지 신호에서 두 렌더러 모두 flush≈0(시리얼 fps/dirty 로그로 확인), 시각 무회귀.

## 건② TRS 6키 저항 래더 입력 (하드웨어 게이트 — 케이블 연결 시점에 투입)
- 전제: ASSEMBLY §Step5.5 하드웨어 존재. **UP버튼(G4) 물리 제거 후** 활성화.
- `app_main.c` 입력단에 컴파일 스위치 `#define INPUT_TRS_LADDER 0`(기본 0=현행 GPIO 6버튼).
  1로 켜면: G4를 **ADC1_CH3 원샷**으로 읽어 전압 구간표(6단+무입력)로 키 판정 →
  기존 디바운스/오토리피트/이벤트 발행 로직 **재사용**(EV_UP..EV_HOME). GPIO 6버튼 경로 비활성.
  FOOTSW(G17)·게인스위치는 GPIO 그대로.
- 구간표는 상단 #define 테이블(래더 0/1k/2.2k/4.7k/10k/22k + Rtop 3.3k 기준 이론값 주석,
  ±8% 허용창). 판정 불가 전압=무입력 처리. 시리얼에 1초 raw mV 로그(튜닝용, 매크로로 on/off).
- 판정: 6키 각각 정확 인식·오인식 0(경계 전압 로그 확인), 기본 0 빌드는 현행과 동일.
