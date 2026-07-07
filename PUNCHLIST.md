# PUNCHLIST.md — 미결·확인 항목 (다음 세션 최우선 점검표)

## 🔴 반복 미결 (즉시)
1. **`hardware/*.pretty/` 풋프린트 폴더가 여전히 미커밋(3회째 404).** GitHub Desktop Changes에
   `.kicad_mod` 4개가 잡히는지 확인 후 커밋. (심볼·노트는 커밋됨 — 단 `kicad_sym`이 SSOT 재작성본인지 확인)
2. **"어떤 버튼 누르면 IDF 에러" 원문 미캡처** (예전 관찰, 이후 재확인 안 됨). 재현되면 로그 캡처.

## 🟡 결정/실측 대기
3. SD 어댑터 VCC: 3.3직결 vs 5V+온보드LDO (보류 중)
4. 커스텀 풋프린트 실측 3+1종: DevKit 행간·6.35잭·MP1584·9V커넥터 (전부 DRAFT 치수)
5. 게인 저항 튜닝: INST 2.2k / LINE 15k는 시작점 — 실기 클리핑 보며 조정
6. `CONFIG_LV_DEF_REFR_PERIOD=33ms`(fps 상한 30) → 15ms 인하는 태윤 승인 후
7. 코덱 모듈 선정: PCM5102A(출력만) vs ES8388(입출력) — S2 착수 게이트
8. 온셋 검출 상수(임계 1.8×·불응 80ms) 실기 튜닝

## 🟢 관찰
9. I2S RX overflow 카운트가 1에서 증가하는지 (부팅 1회는 무해)
10. NVS v1→v2 범프 시 1회 기본값 리셋 발생(정상) — 배치 다시 잡으면 됨

## Codex 투입 순서 (재확인)
3BC 런처·테마 → music_events+Bounce → S1 시뮬레이터 → (병행소형) backlog_minor 건①
→ 건②는 TRS 케이블 연결 시점
