#include "gadget_app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "app_slots.h"
#include "content_screen.h"
#include "storage.h"
#include "theme.h"

static int s_image;
static int s_image_count;
static storage_item_t *s_images;
static char s_image_source[STORAGE_PATH_MAX + 3];

static const ui_theme_t *images_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_IMAGES));
}

static bool images_ensure_catalog(void)
{
    if (s_images) return true;
    s_images = heap_caps_calloc(
        STORAGE_MAX_ITEMS, sizeof(*s_images),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return s_images != NULL;
}

int images_app_count(void)
{
    if (!images_ensure_catalog()) return 1;
    if (s_image_count <= 0) {
        s_image_count = storage_scan(
            STORAGE_MEDIA_IMAGE, s_images, STORAGE_MAX_ITEMS);
    }
    return s_image_count > 0 ? s_image_count : 1;
}

void images_app_set_content(int content)
{
    s_image = content < 0 ? 0 : content;
}

static bool has_gif_extension(const char *path)
{
    const char *extension = path ? strrchr(path, '.') : NULL;
    if (!extension) return false;
    const char *gif = ".gif";
    while (*extension && *gif) {
        if (tolower((unsigned char)*extension) != *gif) return false;
        extension++;
        gif++;
    }
    return *extension == '\0' && *gif == '\0';
}

static void images_show_empty(void)
{
    if (!s_images) {
        content_show_wallpaper("GG", "Gallery memory unavailable");
    } else if (storage_ready()) {
        content_show_wallpaper("GG", "No images");
    } else {
        content_show_wallpaper("GG", storage_status());
    }
}

static void images_show_current(void)
{
    if (s_image_count <= 0) {
        images_show_empty();
        return;
    }
    if (s_image >= s_image_count) s_image = s_image_count - 1;

    const storage_item_t *item = &s_images[s_image];
    const int length = snprintf(
        s_image_source, sizeof(s_image_source), "S:%s", item->path);
    if (length < 0 || length >= (int)sizeof(s_image_source)) {
        images_show_empty();
        return;
    }

    if (has_gif_extension(item->path)) {
        content_show_gif(s_image_source, item->name);
    } else {
        content_show_image(s_image_source, item->name);
    }
}

static void images_refresh(void)
{
    if (!images_ensure_catalog()) {
        s_image_count = 0;
        images_show_empty();
        return;
    }
    s_image_count = storage_scan(
        STORAGE_MEDIA_IMAGE, s_images, STORAGE_MAX_ITEMS);
    if (s_image_count <= 0) {
        s_image = 0;
    } else if (s_image >= s_image_count) {
        s_image = s_image_count - 1;
    }
    images_show_current();
}

static void images_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_SPECTRUM);
    content_screen_apply_theme(images_theme());
    content_screen_create();
    images_refresh();
}

static void images_exit(void)
{
    content_screen_destroy();
}

static bool images_on_event(ui_event_t event)
{
    if (event == EV_LEFT) {
        if (s_image_count <= 0) {
            images_refresh();
            return true;
        }
        s_image = (s_image - 1 + s_image_count) % s_image_count;
        images_show_current();
        return true;
    }
    if (event == EV_RIGHT) {
        if (s_image_count <= 0) {
            images_refresh();
            return true;
        }
        s_image = (s_image + 1) % s_image_count;
        images_show_current();
        return true;
    }
    if (event == EV_OK) {
        images_refresh();
        return true;
    }
    return false;
}

static void images_appearance_changed(void)
{
    content_screen_apply_theme(images_theme());
}

static int images_mode_count(void)
{
    return 1;
}

static const char *images_mode_name(int idx)
{
    return idx == 0 ? "Gallery" : "";
}

static int images_mode_index(void)
{
    return 0;
}

static void images_mode_set(int idx)
{
    (void)idx;
}

const gadget_app_t APP_IMAGES = {
    .id = "images",
    .name = "Gallery",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = images_enter,
    .on_exit = images_exit,
    .on_event = images_on_event,
    .on_appearance_changed = images_appearance_changed,
    .mode_count = images_mode_count,
    .mode_name = images_mode_name,
    .mode_index = images_mode_index,
    .mode_set = images_mode_set,
};
