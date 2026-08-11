/* ============================================================================
 *  content_screen.c  —  image/GIF/text screen and LVGL VFS bridge
 * ========================================================================== */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "content_screen.h"
#include "storage.h"

#define SCR_W      480
#define SCR_H      320
#define FS_LETTER  'S'

/* ---------- LVGL filesystem -> ESP-IDF VFS ------------------------------- */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    const bool read = (mode & LV_FS_MODE_RD) != 0;
    const bool write = (mode & LV_FS_MODE_WR) != 0;
    const char *open_mode = read && write ? "rb+" : (write ? "wb" : "rb");
    return storage_open(path, open_mode);
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file)
{
    (void)drv;
    return file && fclose((FILE *)file) == 0 ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file, void *buf,
                           uint32_t bytes_to_read, uint32_t *bytes_read)
{
    (void)drv;
    if (!file || !buf || !bytes_read) return LV_FS_RES_INV_PARAM;
    *bytes_read = (uint32_t)fread(buf, 1, bytes_to_read, (FILE *)file);
    return ferror((FILE *)file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file, uint32_t pos,
                           lv_fs_whence_t whence)
{
    (void)drv;
    if (!file || pos > (uint32_t)LONG_MAX) return LV_FS_RES_INV_PARAM;
    int origin;
    if (whence == LV_FS_SEEK_SET) origin = SEEK_SET;
    else if (whence == LV_FS_SEEK_CUR) origin = SEEK_CUR;
    else if (whence == LV_FS_SEEK_END) origin = SEEK_END;
    else return LV_FS_RES_INV_PARAM;
    return fseek((FILE *)file, (long)pos, origin) == 0
           ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file, uint32_t *pos)
{
    (void)drv;
    if (!file || !pos) return LV_FS_RES_INV_PARAM;
    long value = ftell((FILE *)file);
    if (value < 0 || (unsigned long)value > UINT32_MAX) return LV_FS_RES_FS_ERR;
    *pos = (uint32_t)value;
    return LV_FS_RES_OK;
}

static lv_fs_drv_t s_fsdrv;

void content_fs_register(void)
{
    lv_fs_drv_init(&s_fsdrv);
    s_fsdrv.letter = FS_LETTER;
    s_fsdrv.open_cb = fs_open;
    s_fsdrv.close_cb = fs_close;
    s_fsdrv.read_cb = fs_read;
    s_fsdrv.seek_cb = fs_seek;
    s_fsdrv.tell_cb = fs_tell;
    lv_fs_drv_register(&s_fsdrv);
}

/* ---------- Screen lifecycle --------------------------------------------- */
static lv_obj_t *s_root;
static lv_obj_t *s_holder;
static lv_obj_t *s_obj;
static lv_obj_t *s_top_bar;
static lv_obj_t *s_bottom_bar;
static lv_obj_t *s_title;
static lv_obj_t *s_counter;
static lv_obj_t *s_name;
static lv_obj_t *s_detail;
static lv_obj_t *s_nav_left;
static lv_obj_t *s_nav_right;
static lv_obj_t *s_status_icon;
static lv_obj_t *s_status_title;
static lv_obj_t *s_status_detail;
static const ui_theme_t *s_theme;
static content_view_t s_view;
static bool s_chrome_visible = true;

static void panel_style(lv_obj_t *obj, lv_opa_t bg_opa, uint32_t color)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, bg_opa, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
}

static void root_delete_cb(lv_event_t *event)
{
    (void)event;
    s_root = NULL;
    s_holder = NULL;
    s_obj = NULL;
    s_top_bar = NULL;
    s_bottom_bar = NULL;
    s_title = NULL;
    s_counter = NULL;
    s_name = NULL;
    s_detail = NULL;
    s_nav_left = NULL;
    s_nav_right = NULL;
    s_status_icon = NULL;
    s_status_title = NULL;
    s_status_detail = NULL;
    s_view = CONTENT_VIEW_NONE;
    s_chrome_visible = true;
}

void content_screen_destroy(void)
{
    if (s_root) lv_obj_delete(s_root);
    s_root = s_holder = s_obj = NULL;
    s_top_bar = s_bottom_bar = NULL;
    s_title = s_counter = s_name = s_detail = NULL;
    s_nav_left = s_nav_right = NULL;
    s_status_icon = s_status_title = s_status_detail = NULL;
    s_view = CONTENT_VIEW_NONE;
    s_chrome_visible = true;
}

void content_screen_apply_theme(const ui_theme_t *theme)
{
    if (!theme) return;
    s_theme = theme;
    if (s_root) {
        lv_obj_set_style_bg_color(s_root, theme->bg, 0);
    }
    if (s_top_bar) lv_obj_set_style_bg_color(s_top_bar, theme->surface, 0);
    if (s_bottom_bar) {
        lv_obj_set_style_bg_color(s_bottom_bar, theme->surface, 0);
    }
    if (s_title) lv_obj_set_style_text_color(s_title, theme->accent, 0);
    if (s_counter) lv_obj_set_style_text_color(s_counter, theme->text, 0);
    if (s_name) lv_obj_set_style_text_color(s_name, theme->text, 0);
    if (s_detail) lv_obj_set_style_text_color(s_detail, theme->text, 0);
    if (s_nav_left) lv_obj_set_style_text_color(s_nav_left, theme->accent, 0);
    if (s_nav_right) lv_obj_set_style_text_color(s_nav_right, theme->accent, 0);
    if (s_status_icon) {
        lv_obj_set_style_text_color(
            s_status_icon,
            s_view == CONTENT_VIEW_ERROR ? theme->accent2 : theme->accent, 0);
    }
    if (s_status_title) {
        lv_obj_set_style_text_color(s_status_title, theme->text, 0);
    }
    if (s_status_detail) {
        lv_obj_set_style_text_color(s_status_detail, theme->text, 0);
    }
    if (s_obj && s_view == CONTENT_VIEW_WALLPAPER) {
        lv_obj_set_style_text_color(s_obj, theme->grid, 0);
    } else if (s_obj && s_view == CONTENT_VIEW_STATUS && !s_status_title) {
        lv_obj_set_style_text_color(s_obj, theme->text, 0);
    }
}

static void clear_content(void)
{
    if (s_obj) {
        lv_obj_delete(s_obj);
        s_obj = NULL;
    }
    s_status_icon = NULL;
    s_status_title = NULL;
    s_status_detail = NULL;
    s_view = CONTENT_VIEW_NONE;
}

void content_screen_create(void)
{
    if (s_root) content_screen_destroy();
    const ui_theme_t *theme = s_theme ? s_theme : theme_get();

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_add_event_cb(s_root, root_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_pos(s_root, 0, 0);
    panel_style(
        s_root, LV_OPA_COVER,
        lv_color_to_u32(theme->bg) & 0xffffffU);

    s_holder = lv_obj_create(s_root);
    lv_obj_set_size(s_holder, SCR_W, SCR_H);
    lv_obj_center(s_holder);
    panel_style(s_holder, LV_OPA_TRANSP, 0x000000);

    s_top_bar = lv_obj_create(s_root);
    lv_obj_set_pos(s_top_bar, 0, 0);
    lv_obj_set_size(s_top_bar, SCR_W, 36);
    panel_style(s_top_bar, LV_OPA_90, 0x000000);

    s_title = lv_label_create(s_top_bar);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_title, 14, 10);

    s_counter = lv_label_create(s_top_bar);
    lv_obj_set_width(s_counter, 120);
    lv_obj_set_style_text_align(s_counter, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(s_counter, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_counter, 346, 11);

    s_bottom_bar = lv_obj_create(s_root);
    lv_obj_set_pos(s_bottom_bar, 0, 272);
    lv_obj_set_size(s_bottom_bar, SCR_W, 48);
    panel_style(s_bottom_bar, LV_OPA_90, 0x000000);

    s_nav_left = lv_label_create(s_bottom_bar);
    lv_label_set_text(s_nav_left, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(s_nav_left, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_nav_left, 12, 14);

    s_name = lv_label_create(s_bottom_bar);
    lv_label_set_long_mode(s_name, LV_LABEL_LONG_DOT);
    lv_obj_set_size(s_name, 372, 18);
    lv_obj_set_style_text_font(s_name, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_name, 44, 5);

    s_detail = lv_label_create(s_bottom_bar);
    lv_label_set_long_mode(s_detail, LV_LABEL_LONG_DOT);
    lv_obj_set_size(s_detail, 372, 16);
    lv_obj_set_style_text_font(s_detail, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(s_detail, LV_OPA_60, 0);
    lv_obj_set_pos(s_detail, 44, 26);

    s_nav_right = lv_label_create(s_bottom_bar);
    lv_label_set_text(s_nav_right, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(s_nav_right, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_nav_right, 444, 14);

    content_screen_set_chrome("GALLERY", "0 / 0", "", "", false);
    content_screen_set_chrome_visible(true);
    content_screen_apply_theme(theme);
}

void content_screen_set_chrome(const char *title, const char *counter,
                               const char *name, const char *detail,
                               bool navigation)
{
    if (!s_root) return;
    lv_label_set_text(s_title, title ? title : "");
    lv_label_set_text(s_counter, counter ? counter : "");
    lv_label_set_text(s_name, name ? name : "");
    lv_label_set_text(s_detail, detail ? detail : "");
    if (navigation) {
        lv_obj_remove_flag(s_nav_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_nav_right, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_nav_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_nav_right, LV_OBJ_FLAG_HIDDEN);
    }
}

void content_screen_set_chrome_visible(bool visible)
{
    s_chrome_visible = visible;
    if (!s_top_bar || !s_bottom_bar) return;
    if (visible) {
        lv_obj_remove_flag(s_top_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_bottom_bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_top_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_bottom_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

bool content_image_get_info(const char *file, bool gif,
                            uint32_t *width, uint32_t *height)
{
    if (!file || !width || !height) return false;
    if (gif) {
        uint16_t gif_width = 0;
        uint16_t gif_height = 0;
        if (!lv_gif_get_size(file, &gif_width, &gif_height)) return false;
        *width = gif_width;
        *height = gif_height;
        return gif_width > 0 && gif_height > 0;
    }

    lv_image_header_t header;
    if (lv_image_decoder_get_info(file, &header) != LV_RESULT_OK ||
        header.w == 0 || header.h == 0) {
        return false;
    }
    *width = header.w;
    *height = header.h;
    return true;
}

void content_show_image(const char *file, const char *name)
{
    if (!s_holder || !file) return;
    clear_content();
    s_obj = lv_image_create(s_holder);
    lv_image_set_src(s_obj, file);
    lv_image_set_inner_align(s_obj, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_size(s_obj, SCR_W, SCR_H);
    lv_obj_center(s_obj);
    s_view = CONTENT_VIEW_IMAGE;
    if (name) lv_label_set_text(s_name, name);
}

void content_show_gif(const char *file, const char *name)
{
    if (!s_holder || !file) return;
    clear_content();
    s_obj = lv_gif_create(s_holder);
    lv_gif_set_src(s_obj, file);
    lv_obj_center(s_obj);
    s_view = CONTENT_VIEW_GIF;
    if (name) lv_label_set_text(s_name, name);
}

void content_show_text(const char *text, const char *name)
{
    if (!s_holder || !text) return;
    clear_content();
    s_obj = lv_label_create(s_holder);
    lv_label_set_long_mode(s_obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_obj, SCR_W - 40);
    lv_label_set_text(s_obj, text);
    lv_obj_set_style_text_color(s_obj, s_theme ? s_theme->text
                                               : theme_get()->text, 0);
    lv_obj_set_style_text_font(s_obj, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(s_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_obj);
    s_view = CONTENT_VIEW_STATUS;
    if (name) lv_label_set_text(s_name, name);
}

void content_show_wallpaper(const char *text, const char *name)
{
    if (!s_holder || !text) return;
    clear_content();
    s_obj = lv_label_create(s_holder);
    lv_label_set_text(s_obj, text);
    lv_obj_set_style_text_color(
        s_obj, s_theme ? s_theme->grid : theme_get()->grid, 0);
    lv_obj_set_style_text_font(s_obj, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_align(s_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(s_obj, LV_OPA_60, 0);
    lv_obj_center(s_obj);
    s_view = CONTENT_VIEW_WALLPAPER;
    if (name) lv_label_set_text(s_name, name);
}

void content_show_status(const char *symbol, const char *status,
                         const char *detail, bool error)
{
    if (!s_holder) return;
    clear_content();
    s_obj = lv_obj_create(s_holder);
    lv_obj_set_size(s_obj, SCR_W, SCR_H);
    lv_obj_center(s_obj);
    panel_style(s_obj, LV_OPA_TRANSP, 0x000000);

    s_status_icon = lv_label_create(s_obj);
    lv_label_set_text(s_status_icon, symbol ? symbol : LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_font(s_status_icon, &lv_font_montserrat_28, 0);
    lv_obj_set_width(s_status_icon, 80);
    lv_obj_set_style_text_align(s_status_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status_icon, 200, 94);

    s_status_title = lv_label_create(s_obj);
    lv_label_set_text(s_status_title, status ? status : "");
    lv_obj_set_style_text_font(s_status_title, &lv_font_montserrat_14, 0);
    lv_obj_set_width(s_status_title, 400);
    lv_obj_set_style_text_align(s_status_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status_title, 40, 137);

    s_status_detail = lv_label_create(s_obj);
    lv_label_set_text(s_status_detail, detail ? detail : "");
    lv_label_set_long_mode(s_status_detail, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(s_status_detail, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(s_status_detail, LV_OPA_60, 0);
    lv_obj_set_width(s_status_detail, 400);
    lv_obj_set_style_text_align(s_status_detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status_detail, 40, 168);

    s_view = error ? CONTENT_VIEW_ERROR : CONTENT_VIEW_STATUS;
    content_screen_apply_theme(s_theme ? s_theme : theme_get());
}

#ifdef PEDAL_SIM
bool content_screen_debug_wallpaper_visible(void)
{
    return s_obj && s_view == CONTENT_VIEW_WALLPAPER &&
           strcmp(lv_label_get_text(s_obj), "GG") == 0;
}

content_view_t content_screen_debug_view(void)
{
    return s_view;
}

const char *content_screen_debug_name(void)
{
    return s_name ? lv_label_get_text(s_name) : "";
}

const char *content_screen_debug_counter(void)
{
    return s_counter ? lv_label_get_text(s_counter) : "";
}

const char *content_screen_debug_status(void)
{
    return s_status_title ? lv_label_get_text(s_status_title) : "";
}

bool content_screen_debug_chrome_visible(void)
{
    return s_chrome_visible;
}
#endif
