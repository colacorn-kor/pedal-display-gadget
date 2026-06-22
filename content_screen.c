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

#define SCR_W      480
#define SCR_H      320
#define SD_MOUNT   "/sdcard"
#define FS_LETTER  'S'

/* ---------- LVGL filesystem -> ESP-IDF VFS ------------------------------- */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    char full[256];
    int written = snprintf(full, sizeof(full), "%s/%s", SD_MOUNT, path);
    if (written < 0 || written >= (int)sizeof(full)) return NULL;

    const bool read = (mode & LV_FS_MODE_RD) != 0;
    const bool write = (mode & LV_FS_MODE_WR) != 0;
    const char *open_mode = read && write ? "rb+" : (write ? "wb" : "rb");
    return fopen(full, open_mode);
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
static lv_obj_t *s_name;
static lv_timer_t *s_name_timer;

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
    if (s_name_timer) {
        lv_timer_delete(s_name_timer);
        s_name_timer = NULL;
    }
    s_root = NULL;
    s_holder = NULL;
    s_obj = NULL;
    s_name = NULL;
}

void content_screen_destroy(void)
{
    if (s_name_timer) {
        lv_timer_delete(s_name_timer);
        s_name_timer = NULL;
    }
    if (s_root) lv_obj_delete(s_root);
    s_root = s_holder = s_obj = s_name = NULL;
}

static void clear_content(void)
{
    if (s_obj) {
        lv_obj_delete(s_obj);
        s_obj = NULL;
    }
}

static void name_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_name) lv_obj_add_flag(s_name, LV_OBJ_FLAG_HIDDEN);
    s_name_timer = NULL;
}

static void flash_name(const char *text)
{
    if (!text || !s_name) return;
    lv_label_set_text(s_name, text);
    lv_obj_remove_flag(s_name, LV_OBJ_FLAG_HIDDEN);

    if (s_name_timer) {
        lv_timer_delete(s_name_timer);
        s_name_timer = NULL;
    }
    s_name_timer = lv_timer_create(name_hide_cb, 2500, NULL);
    if (s_name_timer) lv_timer_set_repeat_count(s_name_timer, 1);
}

void content_screen_create(void)
{
    if (s_root) content_screen_destroy();

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_add_event_cb(s_root, root_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_pos(s_root, 0, 0);
    panel_style(s_root, LV_OPA_COVER, 0x0E1116);

    s_holder = lv_obj_create(s_root);
    lv_obj_set_size(s_holder, SCR_W, SCR_H);
    lv_obj_center(s_holder);
    panel_style(s_holder, LV_OPA_TRANSP, 0x000000);

    s_name = lv_label_create(s_root);
    lv_obj_set_style_text_color(s_name, lv_color_hex(0x6B7480), 0);
    lv_obj_set_style_text_font(s_name, &lv_font_montserrat_14, 0);
    lv_obj_align(s_name, LV_ALIGN_BOTTOM_LEFT, 14, -8);
    lv_obj_add_flag(s_name, LV_OBJ_FLAG_HIDDEN);
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
    flash_name(name);
}

void content_show_gif(const char *file, const char *name)
{
    if (!s_holder || !file) return;
    clear_content();
    s_obj = lv_gif_create(s_holder);
    lv_gif_set_src(s_obj, file);
    lv_obj_center(s_obj);
    flash_name(name);
}

void content_show_text(const char *text, const char *name)
{
    if (!s_holder || !text) return;
    clear_content();
    s_obj = lv_label_create(s_holder);
    lv_label_set_long_mode(s_obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_obj, SCR_W - 40);
    lv_label_set_text(s_obj, text);
    lv_obj_set_style_text_color(s_obj, lv_color_hex(0xF2F4F7), 0);
    lv_obj_set_style_text_font(s_obj, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(s_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_obj);
    flash_name(name);
}
