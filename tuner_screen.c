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

#define C_AMBER 0xE8C24A
#define C_GREEN 0x36C26B
#define C_DIM   0x5A626C

static lv_obj_t *s_root,*s_note,*s_needle,*s_read,*s_zone;

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
static void small_label(const char*s,int x,int y,uint32_t c,const lv_font_t*f){
    lv_obj_t*l=lv_label_create(s_root); lv_label_set_text(l,s);
    lv_obj_set_style_text_color(l,lv_color_hex(c),0);
    lv_obj_set_style_text_font(l,f,0); lv_obj_set_pos(l,x,y);
}

void tuner_screen_create(void){
    s_root=lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root,SCR_W,320); lv_obj_set_pos(s_root,0,0);
    rect(s_root,0x0E1116);

    small_label("TUNER",30,22,0x7FD4A8,&lv_font_montserrat_14);

    /* 인튠 존 ±5센트 */
    s_zone=lv_obj_create(s_root);
    int zw=(int)(5.0f/50.0f*HALF)*2;             /* ±5¢ 폭 */
    lv_obj_set_size(s_zone,zw,22); lv_obj_set_pos(s_zone,CX-zw/2,TRK_Y-11);
    rect(s_zone,0x14301F);
    lv_obj_set_style_bg_color(s_zone,lv_color_hex(C_GREEN),0);
    lv_obj_set_style_bg_opa(s_zone,40,0);

    /* 트랙 + 틱 */
    lv_obj_t*trk=lv_obj_create(s_root); lv_obj_set_size(trk,2*HALF,2);
    lv_obj_set_pos(trk,CX-HALF,TRK_Y-1); rect(trk,0x3A424C);
    for(int c=-50;c<=50;c+=25){
        int x=CX+(int)((float)c/50.0f*HALF);
        tick(x,(c==0)?20:12,(c==0)?0x7FD4A8:0x3A424C);
    }
    /* ASCII avoids relying on optional music-symbol glyphs/font size 20. */
    small_label("b",34,196,0x6B7480,&lv_font_montserrat_14);
    small_label("#",436,196,0x6B7480,&lv_font_montserrat_14);

    /* 음 이름 (큰 글자) */
    s_note=lv_label_create(s_root); lv_label_set_text(s_note,"-");
    lv_obj_set_style_text_color(s_note,lv_color_hex(C_DIM),0);
    lv_obj_set_style_text_font(s_note,&lv_font_montserrat_48,0);
    lv_obj_align(s_note,LV_ALIGN_TOP_MID,0,55);

    /* 니들 */
    s_needle=lv_obj_create(s_root); lv_obj_set_size(s_needle,4,40);
    lv_obj_set_pos(s_needle,CX-2,NDL_Y); rect(s_needle,C_AMBER);
    lv_obj_set_style_radius(s_needle,2,0);
    lv_obj_add_flag(s_needle,LV_OBJ_FLAG_HIDDEN);

    /* 수치 */
    s_read=lv_label_create(s_root); lv_label_set_text(s_read,"-");
    lv_obj_set_style_text_color(s_read,lv_color_hex(0xAEB6C0),0);
    lv_obj_set_style_text_font(s_read,&lv_font_montserrat_14,0);
    lv_obj_align(s_read,LV_ALIGN_BOTTOM_MID,0,-26);
}

void tuner_screen_destroy(void){
    if(s_root) lv_obj_delete(s_root);
    s_root=s_note=s_needle=s_read=s_zone=NULL;
}

void tuner_screen_update(int voiced,const char*name,int octave,float cents,float f0){
    if(!s_root) return;
    if(!voiced){
        lv_label_set_text(s_note,"-");
        lv_obj_set_style_text_color(s_note,lv_color_hex(C_DIM),0);
        lv_obj_add_flag(s_needle,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_read,"-");
        return;
    }
    int intune = (cents>=-5.0f && cents<=5.0f);
    uint32_t col = intune ? C_GREEN : C_AMBER;

    lv_label_set_text_fmt(s_note,"%s%d",name,octave);
    lv_obj_set_style_text_color(s_note,lv_color_hex(col),0);

    float c=cents; if(c<-50)c=-50; if(c>50)c=50;
    int x=CX+(int)(c/50.0f*HALF);
    lv_obj_set_pos(s_needle,x-2,NDL_Y);
    lv_obj_set_style_bg_color(s_needle,lv_color_hex(col),0);
    lv_obj_remove_flag(s_needle,LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text_fmt(s_read,"%+d \xC2\xA2   %.1f Hz",(int)(cents+(cents<0?-0.5f:0.5f)),f0);
}
