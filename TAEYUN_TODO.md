# TAEYUN_TODO.md — 태윤 작업 지시서 (1회용 · 전부 완료하면 이 파일 삭제)

## §1. KiCad 풋프린트 배치 (5분) — .pretty 사건 종결
1. 탐색기에서 폴더 생성: `프로젝트루트\hardware\pedal-display-gadget.pretty\`
2. 이번 채팅에서 내려받은 `.kicad_mod` **4개**를 그 폴더에 넣기:
   `ESP32-S3-DevKitC-1` / `Jack_6.35mm_Mono_Panel` / `MP1584_Module` / `PWR_ELB040202`
3. GitHub Desktop Changes에 4개 잡히는지 **눈으로 확인** → 커밋+푸시.
4. 겸사 확인: KiCad 심볼 라이브러리 열어 `TL072_DIP8`·`PWR_9V_ELB040202`가 보이면
   SSOT 신본. 안 보이면(구초안) 신본 `.kicad_sym`도 교체 필요 — 채팅에서 재요청.

## §2. SD 어댑터 VCC — 결정: **+3V3 직결** (Claude 판단, 근거 포함)
- 근거: ESP32-S3 로직=3.3V, microSD 자체도 3.3V. 레귤레이터/레벨시프터 없는 **순수 어댑터**면
  3.3 직결이 가장 깨끗함. LDO(AMS1117)+시프터 달린 아두이노형 모듈은 5V 급전이 필요하고,
  시프터가 **LCD와 공유하는 40MHz SPI 버스**(SCLK/MOSI)에 부하를 걸어 어렵게 잡은 화면
  안정성을 해칠 위험 → 비권장.
- 모듈 식별법: 뒷면에 3핀 검은 레귤레이터 칩(1117 각인)+콘덴서 있으면 LDO형(비권장),
  배선만 있으면 순수형(3.3 직결 OK). **LDO형만 갖고 있으면 순수형 별도 구매 권장(몇백원대).**
- 배선: `J3.VCC→+3V3`, 나머지는 ASSEMBLY §SD 그대로. VCC 옆에 100nF 하나 붙이면 더 좋음.

## §3. Codex 투입 순서 (각 단계: 지시서 전달→결과 커밋→플래시→완료판정 대조)
1. `CODEX_INSTRUCTION_3BC_launcher_theme.md` → 확인: HOME롱=런처, 테마 즉시적용+재부팅 유지,
   **런처에서 리셋→런처 부팅**, REORDER로 STASH 이동+재부팅 유지, NVS v2 리셋 로그 1회
2. `CODEX_INSTRUCTION_music_events_bounce.md` → 확인: 기타/탭 온셋마다 Bounce 점프,
   음높이로 좌우, bpm 로그 ±10% 수렴, monitor 무회귀
3. `CODEX_INSTRUCTION_pc_simulator.md` → 확인: ESP 빌드 무회귀 먼저(-Werror), 그다음 PC 창
4. `CODEX_INSTRUCTION_backlog_minor.md` 건① (건②는 TRS 케이블 연결 시점)
- 매 단계: 커밋+푸시 후 **Changes 탭 비었는지** 확인(습관).

## §4. 하드웨어 (ASSEMBLY.md 순서대로, 병행 가능)
- INST/LINE 게인 스위치 교체(Step 5) → TRS 컨트롤러 하드웨어(Step 5.5, 케이블 미연결)
- 구매: SPDT, 3.5mm TRS잭×2+케이블, (권장) 순수 SD어댑터, 3.5mm 스테레오잭×2(코덱 자리용)
- **결정 1건**: 코덱 모듈 — PCM5102A(출력만·배선 간단·I2C 불필요) vs ES8388(입출력 통합·
  나중에 AUX입력까지) → S2 착수 게이트. 급하지 않음.

## §5. KiCad 연습 루프 (본인 페이스)
스키매틱 배선 → `.net` export → 다음 세션(Opus)에 붙여넣기 → NETLIST_SPEC diff 검토.

## 완료 조건
§1~§3 끝나고 §4 조립분 반영되면 **이 파일 삭제 + PUNCHLIST.md 해당 항목 지우기.**
