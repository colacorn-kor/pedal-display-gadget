#include "gadget_app.h"

#include "app_slots.h"
#include "content_screen.h"
#include "theme.h"

static const char *IMG_F[] = {
    "S:content/img1.bin", "S:content/img2.bin", "S:content/img3.bin"
};
static const char *IMG_N[] = { "img1", "img2", "img3" };
#define IMGN ((int)(sizeof(IMG_F) / sizeof(IMG_F[0])))

static int s_image;

static const ui_theme_t *images_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_IMAGES));
}

int images_app_count(void)
{
    return IMGN;
}

void images_app_set_content(int content)
{
    s_image = content;
}

static void images_show_current(void)
{
    content_show_image(IMG_F[s_image], IMG_N[s_image]);
}

static void images_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_SPECTRUM);
    content_screen_apply_theme(images_theme());
    content_screen_create();
    images_show_current();
}

static void images_exit(void)
{
    content_screen_destroy();
}

static bool images_on_event(ui_event_t event)
{
    if (event == EV_LEFT) {
        s_image = (s_image - 1 + IMGN) % IMGN;
        images_show_current();
        return true;
    }
    if (event == EV_RIGHT) {
        s_image = (s_image + 1) % IMGN;
        images_show_current();
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
    .name = "Images",
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
