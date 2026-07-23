/* ============================================================================
 *  renderer_reactive.c  —  반응형 캐릭터 렌더러 (renderer_t 구현, 아이디어 #3)
 *  프레임의 bars/level만으로 표현 특징을 자체 추출 → 살아있는 캐릭터.
 *    level    → 몸 크기(숨쉬기) + 입 벌어짐
 *    onset    → 점프 + squash/stretch + 눈 깜빡 (어택/비트)
 *    centroid → 색(저역=따뜻, 고역=차가움) + 시선
 *  파이프라인 변경 0 (자체 완결). 등록만 추가.
 * ========================================================================== */
#include "renderer.h"
#include <math.h>

static lv_obj_t *s_root,*s_body,*s_eyeL,*s_eyeR,*s_mouth;
static viz_theme_t s_t;
static float s_prevlvl=0, s_onset=0;

typedef struct {
    int w, h, x, y, radius;
    uint32_t color;
} obj_state_t;

static obj_state_t s_body_state, s_eyeL_state, s_eyeR_state, s_mouth_state;

static void invalidate_state(obj_state_t *state){
    state->w=-1; state->h=-1; state->x=-1; state->y=-1; state->radius=-1;
    state->color=UINT32_MAX;
}

static void update_geometry(lv_obj_t *obj,obj_state_t *state,
                            int w,int h,int x,int y,int radius){
    if(state->w!=w||state->h!=h){
        lv_obj_set_size(obj,w,h);
        state->w=w; state->h=h;
    }
    if(state->radius!=radius){
        lv_obj_set_style_radius(obj,radius,0);
        state->radius=radius;
    }
    if(state->x!=x||state->y!=y){
        lv_obj_set_pos(obj,x,y);
        state->x=x; state->y=y;
    }
}

static uint32_t lerp_rgb(uint32_t a,uint32_t b,float t){
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int ar=(a>>16)&0xFF,ag=(a>>8)&0xFF,ab=a&0xFF;
    int br=(b>>16)&0xFF,bg=(b>>8)&0xFF,bb=b&0xFF;
    return ((ar+(int)((br-ar)*t))<<16)|((ag+(int)((bg-ag)*t))<<8)|(ab+(int)((bb-ab)*t));
}
static void rs(lv_obj_t*o,uint32_t c){
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(o,0,0); lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_bg_color(o,lv_color_hex(c),0);
}

static void r_create(lv_obj_t*parent,const viz_theme_t*t){
    s_t=*t; s_prevlvl=0; s_onset=0;
    invalidate_state(&s_body_state); invalidate_state(&s_eyeL_state);
    invalidate_state(&s_eyeR_state); invalidate_state(&s_mouth_state);
    s_root=lv_obj_create(parent); lv_obj_set_size(s_root,480,320); lv_obj_set_pos(s_root,0,0); rs(s_root,t->bg);
    s_body =lv_obj_create(s_root); rs(s_body, t->accent);
    s_mouth=lv_obj_create(s_root); rs(s_mouth,t->bg);
    s_eyeL =lv_obj_create(s_root); rs(s_eyeL, 0xFFFFFF);
    s_eyeR =lv_obj_create(s_root); rs(s_eyeR, 0xFFFFFF);
}

static void r_update(const viz_frame_t*f){
    if(!s_root||!f||!f->bars||f->n<2) return;
    /* 특징 추출 */
    float level=f->level, num=0,den=0;
    if(level<0)level=0; else if(level>1)level=1;
    for(int i=0;i<f->n;i++){ num+=i*f->bars[i]; den+=f->bars[i]; }
    float cen = den>1e-6f ? (num/den)/(f->n-1) : 0;          /* 센트로이드 0..1 */
    float d=level-s_prevlvl; if(d<0)d=0; float on=d*4.0f; if(on>1)on=1;
    s_onset=s_onset*0.85f; if(on>s_onset)s_onset=on; s_prevlvl=level;  /* 온셋(감쇠) */

    /* 몸: 크기=레벨, 온셋에 squash, 색=센트로이드, 온셋에 점프 */
    int base=90, sz=base+(int)(level*90);
    int w=sz+(int)(s_onset*30), h=sz-(int)(s_onset*20);
    int cx=240, cy=170-(int)(s_onset*24);
    update_geometry(s_body,&s_body_state,w,h,cx-w/2,cy-h/2,h/2);
    uint32_t body_color=lerp_rgb(0xFF7A4A,0x4AC8FF,cen);
    if(s_body_state.color!=body_color){
        lv_obj_set_style_bg_color(s_body,lv_color_hex(body_color),0);
        s_body_state.color=body_color;
    }

    /* 눈: 깜빡(온셋), 시선(센트로이드) */
    int eye=18, eh=eye-(int)(s_onset*14); if(eh<2)eh=2;
    int ey=cy-h/8-(int)(cen*10);
    update_geometry(s_eyeL,&s_eyeL_state,eye,eh,cx-w/5-eye/2,ey,eh/2);
    update_geometry(s_eyeR,&s_eyeR_state,eye,eh,cx+w/5-eye/2,ey,eh/2);

    /* 입: 레벨에 벌어짐 */
    int mw=w/3, mh=6+(int)(level*40);
    update_geometry(s_mouth,&s_mouth_state,mw,mh,cx-mw/2,cy+h/6,mh/2);
}
static void r_destroy(void){
    if(s_root){ lv_obj_delete(s_root); s_root=0; }
    s_body=0; s_eyeL=0; s_eyeR=0; s_mouth=0;
    invalidate_state(&s_body_state); invalidate_state(&s_eyeL_state);
    invalidate_state(&s_eyeR_state); invalidate_state(&s_mouth_state);
}
const renderer_t RENDERER_REACTIVE = { "reactive", r_create, r_update, r_destroy };
