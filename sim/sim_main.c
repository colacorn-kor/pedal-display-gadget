#include <SDL.h>
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "app.h"
#include "content_screen.h"
#include "platform.h"

bool plat_sim_should_quit(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    lv_init();

    lv_display_t *display = lv_sdl_window_create(480, 320);
    if (!display) return 1;
    lv_sdl_window_set_title(display, "Pedal Display Gadget Simulator");
    lv_sdl_window_set_resizeable(display, false);

    content_fs_register();
    sm_init();

    while (!plat_sim_should_quit()) {
        ui_event_t ev;
        while (plat_input_poll(&ev)) {
            sm_on_event(ev);
        }
        sm_render();

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY || wait_ms > 16) wait_ms = 16;
        if (wait_ms == 0) wait_ms = 1;
        SDL_Delay(wait_ms);
    }

    return 0;
}
