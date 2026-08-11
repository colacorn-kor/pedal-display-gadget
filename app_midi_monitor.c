#include "gadget_app.h"

#include <stdio.h>
#include <string.h>

#include "app_slots.h"
#include "midi_service.h"
#include "theme.h"

#define MONITOR_ROWS 5
#define MIDI_CHANNEL_COUNT 17

static lv_obj_t *s_root;
static lv_obj_t *s_connection;
static lv_obj_t *s_state;
static lv_obj_t *s_latest_type;
static lv_obj_t *s_latest_detail;
static lv_obj_t *s_rows[MONITOR_ROWS];
static lv_obj_t *s_footer;
static int s_channel;
static bool s_paused;
static uint32_t s_clear_before;
static uint32_t s_last_drawn_total = UINT32_MAX;
static midi_service_snapshot_t s_frozen;
static int s_visible_count;

static const ui_theme_t *monitor_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_MIDI_MONITOR));
}

static lv_obj_t *label_at(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, int x, int y,
                          int width, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

static bool channel_matches(const midi_msg_t *message)
{
    if (!message || s_channel == 0) return true;
    switch (message->type) {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF:
    case MIDI_CC:
    case MIDI_PC:
        return message->ch + 1 == s_channel;
    default:
        return true;
    }
}

static void note_name(uint8_t note, char *out, size_t out_size)
{
    static const char *const NAMES[] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B",
    };
    (void)snprintf(out, out_size, "%s%d", NAMES[note % 12],
                   (int)note / 12 - 1);
}

static void format_detail(const midi_msg_t *message,
                          char *out, size_t out_size)
{
    if (!message) {
        (void)snprintf(out, out_size, "Waiting for MIDI");
        return;
    }
    switch (message->type) {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF: {
        char note[8];
        note_name(message->d1, note, sizeof(note));
        (void)snprintf(out, out_size, "CH %u   %s   VEL %u",
                       (unsigned)message->ch + 1u, note,
                       (unsigned)message->d2);
        break;
    }
    case MIDI_CC:
        (void)snprintf(out, out_size, "CH %u   CC %03u   %03u",
                       (unsigned)message->ch + 1u,
                       (unsigned)message->d1,
                       (unsigned)message->d2);
        break;
    case MIDI_PC:
        (void)snprintf(out, out_size, "CH %u   PC %03u",
                       (unsigned)message->ch + 1u,
                       (unsigned)message->d1 + 1u);
        break;
    case MIDI_SPP:
        (void)snprintf(out, out_size, "SONG POSITION %u",
                       (unsigned)message->pos14);
        break;
    default:
        (void)snprintf(out, out_size, "%s", midi_type_name(message->type));
        break;
    }
}

static void style_monitor(void)
{
    if (!s_root) return;
    const ui_theme_t *theme = monitor_theme();
    const lv_color_t muted = lv_color_mix(theme->text, theme->bg, 150);
    lv_obj_set_style_bg_color(s_root, theme->bg, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_text_color(s_root, theme->text, 0);
    lv_obj_set_style_text_color(s_connection, muted, 0);
    lv_obj_set_style_text_color(s_state, theme->accent, 0);
    lv_obj_set_style_text_color(s_latest_type, theme->accent, 0);
    lv_obj_set_style_text_color(s_latest_detail, theme->text, 0);
    for (int i = 0; i < MONITOR_ROWS; i++) {
        lv_obj_set_style_text_color(s_rows[i],
                                    i == 0 ? theme->text : muted, 0);
    }
    lv_obj_set_style_text_color(s_footer, muted, 0);
}

static void draw_snapshot(const midi_service_snapshot_t *snapshot)
{
    if (!s_root || !snapshot) return;
    platform_midi_status_t transport;
    plat_midi_get_status(&transport);

    char text[160];
    (void)snprintf(text, sizeof(text), "MIDI MONITOR  /  %s",
                   transport.input_available ? transport.input_name
                                             : "NO INPUT");
    lv_label_set_text(s_connection, text);
    lv_label_set_text(s_state, s_paused ? "PAUSED" : "LIVE");

    const midi_event_t *latest = NULL;
    int row = 0;
    for (int i = 0; i < snapshot->history_count; i++) {
        const midi_event_t *event = &snapshot->history[i];
        if (event->sequence <= s_clear_before ||
            !channel_matches(&event->message)) {
            continue;
        }
        if (!latest) latest = event;
        if (row < MONITOR_ROWS) {
            char detail[96];
            format_detail(&event->message, detail, sizeof(detail));
            (void)snprintf(text, sizeof(text), "%06u  %-9s  %s",
                           (unsigned)event->timestamp_ms,
                           midi_type_name(event->message.type), detail);
            lv_label_set_text(s_rows[row++], text);
        }
    }
    s_visible_count = row;
    while (row < MONITOR_ROWS) lv_label_set_text(s_rows[row++], "");

    if (latest) {
        lv_label_set_text(s_latest_type,
                          midi_type_name(latest->message.type));
        format_detail(&latest->message, text, sizeof(text));
        lv_label_set_text(s_latest_detail, text);
    } else {
        lv_label_set_text(s_latest_type, "WAITING");
        format_detail(NULL, text, sizeof(text));
        lv_label_set_text(s_latest_detail, text);
    }
    (void)snprintf(text, sizeof(text),
                   "RX %u   CLOCK %u   DROP %u   FILTER %s   OK PAUSE   UP CLEAR",
                   (unsigned)snapshot->total_messages,
                   (unsigned)snapshot->clock_messages,
                   (unsigned)transport.dropped_messages,
                   s_channel == 0 ? "ALL" : "CH");
    if (s_channel != 0) {
        (void)snprintf(text, sizeof(text),
                       "RX %u   CLOCK %u   DROP %u   FILTER CH %d   OK PAUSE   UP CLEAR",
                       (unsigned)snapshot->total_messages,
                       (unsigned)snapshot->clock_messages,
                       (unsigned)transport.dropped_messages, s_channel);
    }
    lv_label_set_text(s_footer, text);
    style_monitor();
}

static void midi_monitor_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_NONE);
    s_channel = app_slots_options(&APP_MIDI_MONITOR);
    if (s_channel < 0 || s_channel >= MIDI_CHANNEL_COUNT) s_channel = 0;
    s_paused = false;
    midi_service_snapshot_t snapshot;
    midi_service_snapshot_get(&snapshot);
    s_clear_before = snapshot.total_messages;
    s_last_drawn_total = UINT32_MAX;
    midi_service_set_capture(MIDI_CAPTURE_MONITOR);

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    s_connection = label_at(s_root, "", &lv_font_montserrat_12,
                            16, 12, 370, LV_TEXT_ALIGN_LEFT);
    s_state = label_at(s_root, "LIVE", &lv_font_montserrat_12,
                       392, 12, 72, LV_TEXT_ALIGN_RIGHT);
    s_latest_type = label_at(s_root, "WAITING", &lv_font_montserrat_28,
                             16, 48, 180, LV_TEXT_ALIGN_LEFT);
    s_latest_detail = label_at(s_root, "Waiting for MIDI",
                               &lv_font_montserrat_14,
                               196, 55, 268, LV_TEXT_ALIGN_RIGHT);
    for (int i = 0; i < MONITOR_ROWS; i++) {
        s_rows[i] = label_at(s_root, "", &lv_font_montserrat_12,
                             16, 103 + i * 31, 448, LV_TEXT_ALIGN_LEFT);
    }
    s_footer = label_at(s_root, "", &lv_font_montserrat_12,
                        16, 282, 448, LV_TEXT_ALIGN_LEFT);
    draw_snapshot(&snapshot);
}

static void midi_monitor_exit(void)
{
    midi_service_set_capture(MIDI_CAPTURE_NONE);
    if (s_root) lv_obj_delete(s_root);
    s_root = s_connection = s_state = s_latest_type = NULL;
    s_latest_detail = s_footer = NULL;
    memset(s_rows, 0, sizeof(s_rows));
}

static void midi_monitor_render(void)
{
    if (s_paused) return;
    midi_service_snapshot_t snapshot;
    midi_service_snapshot_get(&snapshot);
    if (snapshot.total_messages == s_last_drawn_total) return;
    s_last_drawn_total = snapshot.total_messages;
    draw_snapshot(&snapshot);
}

static void set_channel(int channel)
{
    s_channel = channel;
    app_slots_set_options(&APP_MIDI_MONITOR, (uint8_t)channel);
    s_last_drawn_total = UINT32_MAX;
    midi_service_snapshot_t snapshot;
    midi_service_snapshot_get(&snapshot);
    draw_snapshot(s_paused ? &s_frozen : &snapshot);
}

static bool midi_monitor_event(ui_event_t event)
{
    if (event == EV_OK) {
        if (!s_paused) midi_service_snapshot_get(&s_frozen);
        s_paused = !s_paused;
        if (!s_paused) s_last_drawn_total = UINT32_MAX;
        midi_service_snapshot_t live;
        midi_service_snapshot_get(&live);
        draw_snapshot(s_paused ? &s_frozen : &live);
        return true;
    }
    if (event == EV_UP) {
        midi_service_snapshot_t snapshot;
        midi_service_snapshot_get(&snapshot);
        s_clear_before = snapshot.total_messages;
        if (s_paused) s_frozen = snapshot;
        draw_snapshot(s_paused ? &s_frozen : &snapshot);
        return true;
    }
    if (event == EV_LEFT || event == EV_RIGHT) {
        const int delta = event == EV_LEFT ? -1 : 1;
        set_channel((s_channel + delta + MIDI_CHANNEL_COUNT) %
                    MIDI_CHANNEL_COUNT);
        return true;
    }
    if (event == EV_DOWN) return true;
    return false;
}

static void midi_monitor_appearance(void) { style_monitor(); }
static int monitor_mode_count(void) { return 1; }
static const char *monitor_mode_name(int idx)
{
    return idx == 0 ? "Messages" : "";
}
static int monitor_mode_index(void) { return 0; }
static void monitor_mode_set(int idx) { (void)idx; }
static int filter_count(void) { return MIDI_CHANNEL_COUNT; }
static const char *filter_name(int idx)
{
    static char names[16][6];
    if (idx == 0) return "All";
    if (idx < 1 || idx > 16) return "";
    (void)snprintf(names[idx - 1], sizeof(names[0]), "CH %d", idx);
    return names[idx - 1];
}
static int filter_index(void) { return s_channel; }
static void filter_set(int idx)
{
    if (idx >= 0 && idx < MIDI_CHANNEL_COUNT) set_channel(idx);
}

static const app_choice_setting_t MONITOR_SETTINGS[] = {
    {
        .name = "Channel Filter",
        .item_count = filter_count,
        .item_name = filter_name,
        .item_index = filter_index,
        .item_set = filter_set,
    },
};

const gadget_app_t APP_MIDI_MONITOR = {
    .id = "midimon",
    .name = "MIDI Monitor",
    .audio_mode = AUDIO_NONE,
    .on_enter = midi_monitor_enter,
    .on_exit = midi_monitor_exit,
    .on_render = midi_monitor_render,
    .on_event = midi_monitor_event,
    .on_appearance_changed = midi_monitor_appearance,
    .mode_count = monitor_mode_count,
    .mode_name = monitor_mode_name,
    .mode_index = monitor_mode_index,
    .mode_set = monitor_mode_set,
    .choice_settings = MONITOR_SETTINGS,
    .choice_setting_count =
        (int)(sizeof(MONITOR_SETTINGS) / sizeof(MONITOR_SETTINGS[0])),
    .input_sources = APP_INPUT_BUTTONS | APP_INPUT_MIDI,
    .output_routes = APP_OUTPUT_DISPLAY,
};

#ifdef PEDAL_SIM
bool midi_monitor_debug_paused(void) { return s_paused; }
int midi_monitor_debug_channel(void) { return s_channel; }
uint32_t midi_monitor_debug_total(void)
{
    midi_service_snapshot_t snapshot;
    midi_service_snapshot_get(&snapshot);
    return snapshot.total_messages;
}
int midi_monitor_debug_visible_count(void) { return s_visible_count; }
#endif
