#include "gadget_app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "app_slots.h"
#include "content_screen.h"
#include "image_probe.h"
#include "storage.h"
#include "theme.h"

#define GALLERY_DEFER_MS 20u
#define GALLERY_CHROME_TIMEOUT_MS 5000u

typedef enum {
    GALLERY_STATE_SCANNING = 0,
    GALLERY_STATE_LOADING,
    GALLERY_STATE_READY,
    GALLERY_STATE_EMPTY,
    GALLERY_STATE_ERROR,
} gallery_state_t;

typedef enum {
    GALLERY_PENDING_NONE = 0,
    GALLERY_PENDING_SCAN,
    GALLERY_PENDING_LOAD,
} gallery_pending_t;

static int s_image;
static int s_image_count;
static storage_item_t *s_images;
static storage_scan_result_t s_scan_result;
static char s_image_source[STORAGE_PATH_MAX + 3];
static char s_preserved_path[STORAGE_PATH_MAX];
static char s_current_detail[96];
static gallery_state_t s_state = GALLERY_STATE_SCANNING;
static gallery_pending_t s_pending;
static uint32_t s_pending_due_ms;
static uint32_t s_last_interaction_ms;
static bool s_entered;
static bool s_chrome_visible;

static const ui_theme_t *images_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_IMAGES));
}

static void show_chrome(void)
{
    s_last_interaction_ms = plat_millis();
    if (s_chrome_visible) return;
    s_chrome_visible = true;
    content_screen_set_chrome_visible(true);
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
        s_scan_result = storage_scan_ex(
            STORAGE_MEDIA_IMAGE, s_images, STORAGE_MAX_ITEMS);
        s_image_count = s_scan_result.count;
    }
    return s_image_count > 0 ? s_image_count : 1;
}

void images_app_set_content(int content)
{
    s_image = content < 0 ? 0 : content;
    if (s_entered && s_image_count > 0) {
        show_chrome();
        if (s_image >= s_image_count) s_image = s_image_count - 1;
        s_pending = GALLERY_PENDING_LOAD;
        s_pending_due_ms = plat_millis() + GALLERY_DEFER_MS;
    }
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

static const char *filename_from_path(const char *path)
{
    const char *slash = path ? strrchr(path, '/') : NULL;
    return slash ? slash + 1 : (path ? path : "");
}

static void format_counter(char *text, size_t size)
{
    if (s_image_count <= 0) {
        (void)snprintf(text, size, "0 / 0");
        return;
    }
    if (s_scan_result.truncated) {
        (void)snprintf(text, size, "%d / %d+", s_image + 1, s_image_count);
    } else {
        (void)snprintf(text, size, "%d / %d", s_image + 1, s_image_count);
    }
}

static void format_file_size(uint32_t bytes, char *text, size_t size)
{
    if (bytes >= 1024u * 1024u) {
        const uint64_t tenths =
            ((uint64_t)bytes * 10u + 512u * 1024u) / (1024u * 1024u);
        (void)snprintf(text, size, "%lu.%lu MB",
                       (unsigned long)(tenths / 10u),
                       (unsigned long)(tenths % 10u));
    } else if (bytes >= 1024u) {
        const uint64_t tenths = ((uint64_t)bytes * 10u + 512u) / 1024u;
        (void)snprintf(text, size, "%lu.%lu KB",
                       (unsigned long)(tenths / 10u),
                       (unsigned long)(tenths % 10u));
    } else {
        (void)snprintf(text, size, "%lu B", (unsigned long)bytes);
    }
}

static void show_empty(const char *status)
{
    char detail[96];
    if (s_scan_result.skipped_too_long > 0) {
        (void)snprintf(detail, sizeof(detail),
                       "%s  %d path%s skipped", status,
                       s_scan_result.skipped_too_long,
                       s_scan_result.skipped_too_long == 1 ? "" : "s");
    } else {
        (void)snprintf(detail, sizeof(detail), "%s", status);
    }
    content_show_wallpaper("GG", "No images");
    content_screen_set_chrome("GALLERY", "0 / 0", "No images", detail, false);
    s_state = GALLERY_STATE_EMPTY;
}

static void show_loading(void)
{
    char counter[32];
    format_counter(counter, sizeof(counter));
    const char *name = s_image_count > 0
        ? filename_from_path(s_images[s_image].path) : "";
    content_screen_set_chrome(
        "GALLERY", counter, name, "Opening image", s_image_count > 1);
    content_show_status(LV_SYMBOL_REFRESH, "Loading image", name, false);
    s_state = GALLERY_STATE_LOADING;
}

static void show_error(const char *reason)
{
    char counter[32];
    format_counter(counter, sizeof(counter));
    const char *name = s_image_count > 0
        ? filename_from_path(s_images[s_image].path) : "";
    (void)snprintf(s_current_detail, sizeof(s_current_detail), "%s",
                   reason ? reason : "Image unavailable");
    content_screen_set_chrome(
        "GALLERY", counter, name, s_current_detail, s_image_count > 1);
    content_show_status(LV_SYMBOL_WARNING, "Can't open image",
                        s_current_detail, true);
    s_state = GALLERY_STATE_ERROR;
}

static void schedule_load(void)
{
    if (s_image_count <= 0) return;
    if (s_image < 0) s_image = 0;
    if (s_image >= s_image_count) s_image = s_image_count - 1;
    show_loading();
    s_pending = GALLERY_PENDING_LOAD;
    s_pending_due_ms = plat_millis() + GALLERY_DEFER_MS;
}

static void schedule_scan(bool preserve_selection)
{
    s_preserved_path[0] = '\0';
    if (preserve_selection && s_images && s_image_count > 0 &&
        s_image >= 0 && s_image < s_image_count) {
        (void)snprintf(s_preserved_path, sizeof(s_preserved_path), "%s",
                       s_images[s_image].path);
    }
    content_screen_set_chrome(
        "GALLERY", "-- / --", "Refreshing", storage_status(), false);
    content_show_status(LV_SYMBOL_REFRESH, "Scanning library",
                        storage_status(), false);
    s_state = GALLERY_STATE_SCANNING;
    s_pending = GALLERY_PENDING_SCAN;
    s_pending_due_ms = plat_millis() + GALLERY_DEFER_MS;
}

static void process_scan(void)
{
    if (!images_ensure_catalog()) {
        s_image_count = 0;
        show_error("Gallery memory unavailable");
        return;
    }

    s_scan_result = storage_scan_ex(
        STORAGE_MEDIA_IMAGE, s_images, STORAGE_MAX_ITEMS);
    s_image_count = s_scan_result.count;
    if (s_scan_result.status == STORAGE_SCAN_UNAVAILABLE) {
        s_image = 0;
        show_empty(storage_status());
        return;
    }
    if (s_scan_result.status == STORAGE_SCAN_IO_ERROR) {
        s_image = 0;
        show_empty("GG/images unavailable");
        return;
    }
    if (s_image_count <= 0) {
        s_image = 0;
        show_empty("GG/images is empty");
        return;
    }

    int preserved = -1;
    if (s_preserved_path[0]) {
        for (int i = 0; i < s_image_count; i++) {
            if (strcmp(s_images[i].path, s_preserved_path) == 0) {
                preserved = i;
                break;
            }
        }
    }
    if (preserved >= 0) s_image = preserved;
    if (s_image >= s_image_count) s_image = s_image_count - 1;
    if (s_image < 0) s_image = 0;
    schedule_load();
}

static void process_load(void)
{
    if (s_image_count <= 0 || !s_images) {
        show_empty("GG/images is empty");
        return;
    }
    const storage_item_t *item = &s_images[s_image];
    image_probe_result_t probe;
    if (!image_probe_file(item->path, &probe)) {
        show_error(image_probe_status_text(probe.status));
        return;
    }

    const int length = snprintf(
        s_image_source, sizeof(s_image_source), "S:%s", item->path);
    if (length < 0 || length >= (int)sizeof(s_image_source)) {
        show_error("Image path is too long");
        return;
    }

    const bool gif = has_gif_extension(item->path);
    uint32_t decoded_width = 0;
    uint32_t decoded_height = 0;
    if (!content_image_get_info(
            s_image_source, gif, &decoded_width, &decoded_height)) {
        show_error("Decoder rejected image");
        return;
    }

    char counter[32];
    char size[24];
    format_counter(counter, sizeof(counter));
    format_file_size(probe.file_size, size, sizeof(size));
    (void)snprintf(s_current_detail, sizeof(s_current_detail),
                   "%s  %lux%lu  %s", probe.format,
                   (unsigned long)decoded_width,
                   (unsigned long)decoded_height, size);
    content_screen_set_chrome(
        "GALLERY", counter, filename_from_path(item->path),
        s_current_detail, s_image_count > 1);
    if (gif) {
        content_show_gif(s_image_source, filename_from_path(item->path));
    } else {
        content_show_image(s_image_source, filename_from_path(item->path));
    }
    s_state = GALLERY_STATE_READY;
}

static void images_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_SPECTRUM);
    content_screen_apply_theme(images_theme());
    content_screen_create();
    s_entered = true;
    s_chrome_visible = false;
    show_chrome();
    schedule_scan(false);
}

static void images_exit(void)
{
    s_entered = false;
    s_pending = GALLERY_PENDING_NONE;
    s_chrome_visible = true;
    content_screen_destroy();
}

static void images_render(void)
{
    if (!s_entered) return;
    const uint32_t now = plat_millis();
    if (s_pending != GALLERY_PENDING_NONE &&
        (int32_t)(now - s_pending_due_ms) >= 0) {
        const gallery_pending_t action = s_pending;
        s_pending = GALLERY_PENDING_NONE;
        if (action == GALLERY_PENDING_SCAN) process_scan();
        else if (action == GALLERY_PENDING_LOAD) process_load();
    }

    if (s_state == GALLERY_STATE_READY && s_chrome_visible &&
        (uint32_t)(now - s_last_interaction_ms) >=
            GALLERY_CHROME_TIMEOUT_MS) {
        s_chrome_visible = false;
        content_screen_set_chrome_visible(false);
    }
}

static bool images_on_event(ui_event_t event)
{
    show_chrome();
    if (event == EV_LEFT || event == EV_RIGHT) {
        if (s_image_count <= 0) {
            schedule_scan(false);
            return true;
        }
        const int delta = event == EV_LEFT ? -1 : 1;
        s_image = (s_image + delta + s_image_count) % s_image_count;
        schedule_load();
        return true;
    }
    if (event == EV_OK) {
        schedule_scan(true);
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
    .on_render = images_render,
    .on_event = images_on_event,
    .on_appearance_changed = images_appearance_changed,
    .mode_count = images_mode_count,
    .mode_name = images_mode_name,
    .mode_index = images_mode_index,
    .mode_set = images_mode_set,
    .input_sources = APP_INPUT_BUTTONS,
    .output_routes = APP_OUTPUT_DISPLAY,
};

#ifdef PEDAL_SIM
int images_app_debug_state(void)
{
    return (int)s_state;
}

int images_app_debug_count(void)
{
    return s_image_count;
}

int images_app_debug_index(void)
{
    return s_image;
}

const char *images_app_debug_path(void)
{
    if (!s_images || s_image_count <= 0 || s_image < 0 ||
        s_image >= s_image_count) {
        return "";
    }
    return s_images[s_image].path;
}
#endif
