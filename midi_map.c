/* ============================================================================
 *  midi_map.c  —  MIDI messages to thread-safe UI commands
 * ========================================================================== */
#include <stdint.h>

#include "esp_timer.h"
#include "app.h"
#include "midi.h"

typedef struct {
    int content;
    int theme;
    const char *renderer_name;
} scene_t;

static const scene_t SCENES[] = {
    { 0, 0, "curve" },
    { 1, 1, "curve" },
    { 2, 2, "bars" },
    { 0, 1, "reactive" },
};
#define SCENE_COUNT ((int)(sizeof(SCENES) / sizeof(SCENES[0])))

#define CC_NEXT  80
#define CC_PREV  81
#define CC_THEME 82
#define CC_REND  83
#define CC_TUNER 84
#define CC_MUTE  85
#define CC_COUNT 6

static uint8_t s_cc_high[16][CC_COUNT];
static int64_t s_last_clock;
static int64_t s_tick_ema;
static unsigned s_valid_ticks;

static int cc_index(uint8_t controller)
{
    switch (controller) {
    case CC_NEXT:  return 0;
    case CC_PREV:  return 1;
    case CC_THEME: return 2;
    case CC_REND:  return 3;
    case CC_TUNER: return 4;
    case CC_MUTE:  return 5;
    default: return -1;
    }
}

static void handle_cc(const midi_msg_t *message)
{
    int index = cc_index(message->d1);
    if (index < 0) return;

    int high = message->d2 >= 64;
    int rising = high && !s_cc_high[message->ch][index];
    s_cc_high[message->ch][index] = (uint8_t)high;
    if (!rising) return;

    switch (message->d1) {
    case CC_NEXT:  ui_post_event(EV_RIGHT); break;
    case CC_PREV:  ui_post_event(EV_LEFT); break;
    case CC_THEME: ui_post_event(EV_UP); break;
    case CC_REND:  ui_post_event(EV_DOWN); break;
    case CC_TUNER: ui_post_event(EV_FOOTSW_HOLD); break;
    case CC_MUTE:  ui_post_mute_toggle(); break;
    }
}

static void reset_clock_tracking(void)
{
    s_last_clock = 0;
    s_tick_ema = 0;
    s_valid_ticks = 0;
}

void midi_on_message(const midi_msg_t *message)
{
    if (!message) return;

    switch (message->type) {
    case MIDI_PC:
        if (message->d1 < SCENE_COUNT) {
            const scene_t *scene = &SCENES[message->d1];
            ui_post_scene(scene->content, scene->theme, scene->renderer_name);
        }
        break;

    case MIDI_CC:
        handle_cc(message);
        break;

    case MIDI_CLOCK: {
        int64_t now = esp_timer_get_time();
        if (s_last_clock) {
            int64_t delta = now - s_last_clock;
            /* 25..500 BPM guard prevents one transport gap poisoning the EMA. */
            if (delta >= 5000 && delta <= 100000) {
                s_tick_ema = s_tick_ema ? (s_tick_ema * 7 + delta) / 8 : delta;
                s_valid_ticks++;
                if ((s_valid_ticks % 6U) == 0U) {
                    float bpm = 60000000.0f / ((float)s_tick_ema * 24.0f);
                    if (bpm >= 30.0f && bpm <= 300.0f) ui_post_tempo(bpm);
                }
            } else {
                s_tick_ema = 0;
                s_valid_ticks = 0;
            }
        }
        s_last_clock = now;
        break;
    }

    case MIDI_START:
    case MIDI_CONTINUE:
    case MIDI_STOP:
        reset_clock_tracking();
        break;

    case MIDI_SPP:
        /* position is intentionally retained for the future transport layer */
        break;

    default:
        break;
    }
}
