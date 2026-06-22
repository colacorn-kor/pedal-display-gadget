/* ============================================================================
 *  renderer.h  —  시각화 렌더러 인터페이스 (테마 시스템의 토대)
 *
 *  분석 ↔ 렌더 분리:
 *    오디오는 viz_frame_t(데이터 피드)만 만들고,
 *    등록된 renderer_t(곡선·막대·로봇·귀여움…)가 그걸 받아 그린다.
 *  다운로드 테마 = JSON → viz_theme_t(팔레트) + 렌더러 선택.
 * ========================================================================== */
#pragma once
#include "lvgl.h"
#include <stdint.h>

/* 데이터 피드 (분리 지점) */
typedef struct {
    const float *bars;    /* 스펙트럼 0..1, n개 (현재 256) */
    const float *peaks;   /* 피크 0..1, n개 */
    int          n;
    float        level;   /* 전체 레벨 0..1 (반응형 테마용) */
} viz_frame_t;

/* 테마 팔레트/플래그 (다운로드 테마가 채움) */
typedef struct {
    uint32_t bg, grid, accent, line, peak;
    uint32_t lo, mid, hi;        /* 레벨 색상존 */
    int      show_grid, show_axis;
} viz_theme_t;

/* 렌더러 = 이 vtable 구현 + 팔레트 */
typedef struct {
    const char *name;
    void (*create )(lv_obj_t *parent, const viz_theme_t *theme);
    void (*update )(const viz_frame_t *frame);
    void (*destroy)(void);
} renderer_t;

/* 레지스트리 */
void              renderer_register(const renderer_t *r);
int               renderer_count(void);
const renderer_t* renderer_at(int idx);
int               renderer_find(const char *name);   /* 이름→인덱스, 없으면 -1 */
void              renderer_select(int idx, lv_obj_t *parent, const viz_theme_t *theme);
void              renderer_render(const viz_frame_t *frame);
void              renderer_teardown(void);

/* 내장 테마 + 등록 */
void                renderers_init(void);            /* 곡선·막대 등록 */
int                 viz_theme_count(void);
const viz_theme_t*  viz_theme_at(int idx);
const char*         viz_theme_name(int idx);

/* 렌더러 구현이 노출하는 vtable (renderer_*.c) */
extern const renderer_t RENDERER_CURVE;
extern const renderer_t RENDERER_BARS;
extern const renderer_t RENDERER_REACTIVE;
