/* ============================================================================
 *  renderer.c  —  렌더러 레지스트리 + 내장 테마
 * ========================================================================== */
#include "renderer.h"
#include <string.h>

#define MAX_R 8
static const renderer_t *s_list[MAX_R];
static int               s_n = 0;
static const renderer_t *s_active = 0;
static int               s_initialized = 0;

void renderer_register(const renderer_t *r){
    if(!r||!r->name||s_n>=MAX_R) return;
    if(renderer_find(r->name)>=0) return;
    s_list[s_n++]=r;
}
int  renderer_count(void){ return s_n; }
const renderer_t* renderer_at(int i){ return (i>=0&&i<s_n)?s_list[i]:0; }
int  renderer_find(const char *name){
    for(int i=0;i<s_n;i++) if(!strcmp(s_list[i]->name,name)) return i;
    return -1;
}
void renderer_select(int idx, lv_obj_t *parent, const viz_theme_t *theme){
    const renderer_t *next=renderer_at(idx);
    if(!next||!parent||!theme) return;
    if(s_active && s_active->destroy) s_active->destroy();
    s_active = next;
    if(s_active && s_active->create) s_active->create(parent, theme);
}
void renderer_render(const viz_frame_t *f){ if(s_active && s_active->update) s_active->update(f); }
void renderer_teardown(void){ if(s_active && s_active->destroy) s_active->destroy(); s_active=0; }

/* ── 내장 테마 (로봇/귀여움 = 팔레트일 뿐) ───────────────────────────────── */
static const viz_theme_t THEMES[] = {
    /* Classic */
    { .bg=0x0E1116,.grid=0x2A323C,.accent=0x7FD4A8,.line=0xF2F4F7,.peak=0xC9D2DC,
      .lo=0x153A24,.mid=0x4A3C10,.hi=0x4A1A16,.show_grid=1,.show_axis=1 },
    /* Robot (cyan tech) */
    { .bg=0x05080A,.grid=0x10303A,.accent=0x22D3EE,.line=0x9BF6FF,.peak=0xCFFAFE,
      .lo=0x06303A,.mid=0x0A4A55,.hi=0x114E63,.show_grid=1,.show_axis=1 },
    /* Cute (pastel) */
    { .bg=0x1A1322,.grid=0x3A2A45,.accent=0xFFB3D9,.line=0xFFFFFF,.peak=0xFFE0F0,
      .lo=0x3A2540,.mid=0x5A2A55,.hi=0x7A2A60,.show_grid=0,.show_axis=0 },
};
static const char *TNAME[] = { "Classic", "Robot", "Cute" };
#define THEME_N (int)(sizeof(THEMES)/sizeof(THEMES[0]))
_Static_assert(sizeof(THEMES)/sizeof(THEMES[0]) == sizeof(TNAME)/sizeof(TNAME[0]),
               "theme names must match theme definitions");

int                viz_theme_count(void){ return THEME_N; }
const viz_theme_t* viz_theme_at(int i){ return (i>=0&&i<THEME_N)?&THEMES[i]:&THEMES[0]; }
const char*        viz_theme_name(int i){ return (i>=0&&i<THEME_N)?TNAME[i]:TNAME[0]; }

void renderers_init(void){
    if(s_initialized) return;
    s_initialized=1;
    renderer_register(&RENDERER_CURVE);
    renderer_register(&RENDERER_BARS);
    renderer_register(&RENDERER_REACTIVE);
    /* 다운로드 렌더러는 여기에 추가 등록 */
}
