/* ============================================================================
 *  renderer_bars.c  —  막대 렌더러 (renderer_t 구현, "엎었던 막대 테마" 복원)
 *  같은 256점 피드를 32막대로 max-그룹핑해 표시. 렌더러마다 시각 해상도 자유.
 * ========================================================================== */
#include "renderer.h"

#define SCR_W 480
#define TOP 22
#define BOT 18
#define LEFT 30
#define RIGHT 6
#define PX LEFT
#define PY TOP
#define PW (SCR_W-LEFT-RIGHT)
#define PH (320-TOP-BOT)
#define NB 32

static lv_obj_t *s_root, *s_bar[NB], *s_peak[NB];
static viz_theme_t s_t;
static const float slot=(float)PW/NB;
static const int bw=(int)((float)PW/NB-2.4f+0.5f);
static int s_prev_h[NB], s_prev_peak_y[NB];
static uint32_t s_prev_color[NB];
static bool s_prev_peak_hidden[NB];

static void rectstyle(lv_obj_t*o,uint32_t c){
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(o,0,0); lv_obj_set_style_border_width(o,0,0);
    lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_bg_color(o,lv_color_hex(c),0);
}
static uint32_t levelc(float v){ return v<0.60f?s_t.lo:(v<0.85f?s_t.mid:s_t.hi); }

static void b_create(lv_obj_t*parent,const viz_theme_t*t){
    s_t=*t;
    s_root=lv_obj_create(parent); lv_obj_set_size(s_root,SCR_W,320); lv_obj_set_pos(s_root,0,0);
    rectstyle(s_root,t->bg);
    for(int b=0;b<NB;b++){
        int x=PX+(int)(b*slot+1.2f);
        s_bar[b]=lv_obj_create(s_root); lv_obj_set_size(s_bar[b],bw,1);
        lv_obj_set_pos(s_bar[b],x,PY+PH-1); rectstyle(s_bar[b],t->lo);
        s_peak[b]=lv_obj_create(s_root); lv_obj_set_size(s_peak[b],bw,2);
        lv_obj_set_pos(s_peak[b],x,PY+PH); rectstyle(s_peak[b],t->peak);
        lv_obj_add_flag(s_peak[b],LV_OBJ_FLAG_HIDDEN);
        s_prev_h[b]=1;
        s_prev_color[b]=t->lo;
        s_prev_peak_y[b]=PY+PH;
        s_prev_peak_hidden[b]=true;
    }
}
static void b_update(const viz_frame_t*f){
    if(!s_root||!f||!f->bars||f->n<=0) return;
    for(int b=0;b<NB;b++){
        float v=0,pkv=0;                         /* 256→32 max 그룹핑 */
        int begin=b*f->n/NB;
        int end=(b+1)*f->n/NB;
        if(end<=begin) end=begin+1;
        for(int i=begin;i<end&&i<f->n;i++){
            if(f->bars[i]>v)v=f->bars[i];
            if(f->peaks&&f->peaks[i]>pkv)pkv=f->peaks[i];
        }
        if(v<0)v=0; else if(v>1)v=1;
        if(pkv<0)pkv=0; else if(pkv>1)pkv=1;
        int h=(int)(v*PH+0.5f); if(h<1)h=1;
        if(h!=s_prev_h[b]){
            lv_obj_set_size(s_bar[b],bw,h); lv_obj_set_y(s_bar[b],PY+PH-h);
            s_prev_h[b]=h;
        }
        uint32_t color=levelc(v);
        if(color!=s_prev_color[b]){
            lv_obj_set_style_bg_color(s_bar[b],lv_color_hex(color),0);
            s_prev_color[b]=color;
        }
        if(pkv>0.001f){
            int peak_y=PY+(int)((1-pkv)*PH);
            if(peak_y!=s_prev_peak_y[b]){
                lv_obj_set_y(s_peak[b],peak_y);
                s_prev_peak_y[b]=peak_y;
            }
            if(s_prev_peak_hidden[b]){
                lv_obj_remove_flag(s_peak[b],LV_OBJ_FLAG_HIDDEN);
                s_prev_peak_hidden[b]=false;
            }
        } else if(!s_prev_peak_hidden[b]){
            lv_obj_add_flag(s_peak[b],LV_OBJ_FLAG_HIDDEN);
            s_prev_peak_hidden[b]=true;
        }
    }
}
static void b_destroy(void){
    if(s_root){ lv_obj_delete(s_root); s_root=0; }
    for(int b=0;b<NB;b++){
        s_bar[b]=0; s_peak[b]=0;
        s_prev_h[b]=-1; s_prev_color[b]=UINT32_MAX;
        s_prev_peak_y[b]=-1; s_prev_peak_hidden[b]=true;
    }
}
const renderer_t RENDERER_BARS = { "bars", b_create, b_update, b_destroy };
