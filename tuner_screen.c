/* ============================================================================
 *  tuner_screen.c  —  튜너 화면 (LVGL v9, 480×320)
 *  큰 음 이름 + 센트 니들 바 + 인튠 존(±5¢) + 플랫/샤프 + 수치.
 *  인튠이면 초록, 아니면 앰버. 무음이면 "—".
 *
 *  사용: tuner_screen_create()
 *        tuner_screen_update(voiced, name, octave, cents, f0)  // 매 프레임
 *        tuner_screen_destroy()
 *  ⚠ 더 큰 음 글자엔 커스텀 폰트 권장(빌트인 최대 montserrat_48).
 * ========================================================================== */
#include "lvgl.h"
#include "tuner_screen.h"

#define SCR_W 480
#define CX    240
#define HALF  180          /* 센트 스케일 반폭 (±50¢ → ±180px) */
#define TRK_Y 207
#define NDL_Y 187

static lv_obj_t *s_root,*s_note,*s_needle,*s_read,*s_zone;
static lv_obj_t *s_title,*s_track,*s_flat,*s_sharp;
static lv_obj_t *s_ticks[5];
static const ui_theme_t *s_theme;

static void rect(lv_obj_t*o,uint32_t c){
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(o,0,0); lv_obj_set_style_border_width(o,0,0);
    lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(c),0);
}
static lv_obj_t* tick(int x,int h,uint32_t c){
    lv_obj_t*t=lv_obj_create(s_root); lv_obj_set_size(t,2,h);
    lv_obj_set_pos(t,x-1,TRK_Y-h/2); rect(t,c); return t;
}

static uint32_t theme_hex(lv_color_t color)
{
    return lv_color_to_u32(color) & 0xffffffU;
}

void tuner_screen_apply_theme(const ui_theme_t *theme)
{
    if (!theme) return;
    s_theme = theme;
    if (!s_root) return;

    lv_obj_set_style_bg_color(s_root, theme->bg, 0);
    if (s_title) lv_obj_set_style_text_color(s_title, theme->accent, 0);
    if (s_zone) lv_obj_set_style_bg_color(s_zone, theme->accent, 0);
    if (s_track) lv_obj_set_style_bg_color(s_track, theme->grid, 0);
    for (int i = 0; i < 5; i++) {
        if (s_ticks[i]) {
            lv_obj_set_style_bg_color(
                s_ticks[i], i == 2 ? theme->accent : theme->grid, 0);
        }
    }
    if (s_flat) lv_obj_set_style_text_color(s_flat, theme->grid, 0);
    if (s_sharp) lv_obj_set_style_text_color(s_sharp, theme->grid, 0);
    if (s_note) lv_obj_set_style_text_color(s_note, theme->grid, 0);
    if (s_needle) lv_obj_set_style_bg_color(s_needle, theme->accent2, 0);
    if (s_read) lv_obj_set_style_text_color(s_read, theme->text, 0);
}

void tuner_screen_create(void){
    const ui_theme_t *theme = s_theme ? s_theme : theme_get();
    s_root=lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root,SCR_W,320); lv_obj_set_pos(s_root,0,0);
    rect(s_root,theme_hex(theme->bg));

    s_title=lv_label_create(s_root); lv_label_set_text(s_title,"TUNER");
    lv_obj_set_style_text_color(s_title,theme->accent,0);
    lv_obj_set_style_text_font(s_title,&lv_font_montserrat_14,0);
    lv_obj_set_pos(s_title,30,22);

    /* 인튠 존 ±5센트 */
    s_zone=lv_obj_create(s_root);
    int zw=(int)(5.0f/50.0f*HALF)*2;             /* ±5¢ 폭 */
    lv_obj_set_size(s_zone,zw,22); lv_obj_set_pos(s_zone,CX-zw/2,TRK_Y-11);
    rect(s_zone,theme_hex(theme->accent));
    lv_obj_set_style_bg_opa(s_zone,40,0);

    /* 트랙 + 틱 */
    s_track=lv_obj_create(s_root); lv_obj_set_size(s_track,2*HALF,2);
    lv_obj_set_pos(s_track,CX-HALF,TRK_Y-1);
    rect(s_track,theme_hex(theme->grid));
    int tick_idx=0;
    for(int c=-50;c<=50;c+=25){
        int x=CX+(int)((float)c/50.0f*HALF);
        s_ticks[tick_idx++]=tick(
            x,(c==0)?20:12,
            theme_hex(c==0 ? theme->accent : theme->grid));
    }
    /* ASCII avoids relying on optional music-symbol glyphs/font size 20. */
    s_flat=lv_label_create(s_root); lv_label_set_text(s_flat,"b");
    lv_obj_set_style_text_color(s_flat,theme->grid,0);
    lv_obj_set_style_text_font(s_flat,&lv_font_montserrat_14,0);
    lv_obj_set_pos(s_flat,34,196);
    s_sharp=lv_label_create(s_root); lv_label_set_text(s_sharp,"#");
    lv_obj_set_style_text_color(s_sharp,theme->grid,0);
    lv_obj_set_style_text_font(s_sharp,&lv_font_montserrat_14,0);
    lv_obj_set_pos(s_sharp,436,196);

    /* 음 이름 (큰 글자) */
    s_note=lv_label_create(s_root); lv_label_set_text(s_note,"-");
    lv_obj_set_style_text_color(s_note,theme->grid,0);
    lv_obj_set_style_text_font(s_note,&lv_font_montserrat_48,0);
    lv_obj_align(s_note,LV_ALIGN_TOP_MID,0,55);

    /* 니들 */
    s_needle=lv_obj_create(s_root); lv_obj_set_size(s_needle,4,40);
    lv_obj_set_pos(s_needle,CX-2,NDL_Y);
    rect(s_needle,theme_hex(theme->accent2));
    lv_obj_set_style_radius(s_needle,2,0);
    lv_obj_add_flag(s_needle,LV_OBJ_FLAG_HIDDEN);

    /* 수치 */
    s_read=lv_label_create(s_root); lv_label_set_text(s_read,"-");
    lv_obj_set_style_text_color(s_read,theme->text,0);
    lv_obj_set_style_text_font(s_read,&lv_font_montserrat_14,0);
    lv_obj_align(s_read,LV_ALIGN_BOTTOM_MID,0,-26);
}

void tuner_screen_destroy(void){
    if(s_root) lv_obj_delete(s_root);
    s_root=s_note=s_needle=s_read=s_zone=NULL;
    s_title=s_track=s_flat=s_sharp=NULL;
    for(int i=0;i<5;i++) s_ticks[i]=NULL;
}

void tuner_screen_update(int voiced,const char*name,int octave,float cents,float f0){
    if(!s_root) return;
    if(!voiced){
        lv_label_set_text(s_note,"-");
        const ui_theme_t *theme=s_theme?s_theme:theme_get();
        lv_obj_set_style_text_color(s_note,theme->grid,0);
        lv_obj_add_flag(s_needle,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_read,"-");
        return;
    }
    int intune = (cents>=-5.0f && cents<=5.0f);
    const ui_theme_t *theme=s_theme?s_theme:theme_get();
    uint32_t col=theme_hex(intune?theme->accent:theme->accent2);

    lv_label_set_text_fmt(s_note,"%s%d",name,octave);
    lv_obj_set_style_text_color(s_note,lv_color_hex(col),0);

    float c=cents; if(c<-50)c=-50; if(c>50)c=50;
    int x=CX+(int)(c/50.0f*HALF);
    lv_obj_set_pos(s_needle,x-2,NDL_Y);
    lv_obj_set_style_bg_color(s_needle,lv_color_hex(col),0);
    lv_obj_remove_flag(s_needle,LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text_fmt(s_read,"%+d \xC2\xA2   %.1f Hz",(int)(cents+(cents<0?-0.5f:0.5f)),f0);
}
