/* ============================================================================
 *  app_main.c  —  ESP-IDF v5.x integration
 *
 *  Core 1 owns I2S and every DSP state object.
 *  Core 0 owns every LVGL/screen-manager call.
 * ========================================================================== */
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#ifndef INPUT_TRS_LADDER
#define INPUT_TRS_LADDER 1
#endif
#ifndef INPUT_TRS_LOG
#define INPUT_TRS_LOG 1
#endif
#if INPUT_TRS_LADDER
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#endif
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "app.h"
#include "content_screen.h"
#include "display_bringup.h"
#include "fft_map.h"
#include "music_events.h"
#include "tuner.h"

#define DMA_FRAMES  256
#define MUTE_IO      GPIO_NUM_3
#define BTN_UP_IO    GPIO_NUM_4
#define BTN_DOWN_IO  GPIO_NUM_5
#define BTN_LEFT_IO  GPIO_NUM_6
#define BTN_OK_IO    GPIO_NUM_16
#define BTN_RIGHT_IO GPIO_NUM_7
#define BTN_HOME_IO  GPIO_NUM_15
#define FOOTSW_IO    GPIO_NUM_17
#define I2S_MCK      GPIO_NUM_8
#define I2S_BCK      GPIO_NUM_9
#define I2S_WS      GPIO_NUM_18
#define I2S_DIN      GPIO_NUM_10

#define INPUT_POLL_MS         10
#define INPUT_DEBOUNCE_MS     30
#define INPUT_HOLD_MS         500
#define INPUT_REPEAT_DELAY_MS 400
#define INPUT_REPEAT_RATE_MS  120

#if INPUT_TRS_LADDER
#define INPUT_TRS_ADC_SAMPLES 32
#define INPUT_TRS_IDLE_MIN    0.85f
#define INPUT_TRS_IDLE_TRACK  0.01f

/* Rkey 0/150/470/1k/2k/10k with Rtop 4.7k; measured-window margins are 70%. */
#define INPUT_TRS_UP_CENTER     0.00000f
#define INPUT_TRS_UP_WINDOW     0.0070f
#define INPUT_TRS_DOWN_CENTER   0.03093f
#define INPUT_TRS_DOWN_WINDOW   0.0081f
#define INPUT_TRS_LEFT_CENTER   0.09091f
#define INPUT_TRS_LEFT_WINDOW   0.0093f
#define INPUT_TRS_RIGHT_CENTER  0.17544f
#define INPUT_TRS_RIGHT_WINDOW  0.0296f
#define INPUT_TRS_OK_CENTER     0.29851f
#define INPUT_TRS_OK_WINDOW     0.0431f
#define INPUT_TRS_HOME_CENTER   0.68027f
#define INPUT_TRS_HOME_WINDOW   0.1119f
#endif

static const char *TAG = "app";

/* ---------- Cross-core audio publication --------------------------------- */
static audio_viz_snapshot_t s_viz[2];
static _Atomic unsigned     s_viz_seq[2];
static _Atomic int          s_viz_ready;
static _Atomic int          s_audio_mode = AUDIO_SPECTRUM;
static _Atomic int          s_viz_mode = VIZ_MONITOR;

static void publish_empty_viz_frame(int *producer)
{
    int slot = *producer;
    atomic_fetch_add_explicit(&s_viz_seq[slot], 1U, memory_order_acq_rel);
    memset(&s_viz[slot], 0, sizeof(s_viz[slot]));
    atomic_fetch_add_explicit(&s_viz_seq[slot], 1U, memory_order_release);
    atomic_store_explicit(&s_viz_ready, slot, memory_order_release);
    *producer ^= 1;
}

void audio_set_mode(audio_mode_t mode)
{
    if (mode != AUDIO_SPECTRUM && mode != AUDIO_TUNER) return;
    atomic_store_explicit(&s_audio_mode, (int)mode, memory_order_release);
}

audio_mode_t audio_get_mode(void)
{
    return (audio_mode_t)atomic_load_explicit(&s_audio_mode, memory_order_acquire);
}

void audio_set_viz_mode(viz_mode_t mode)
{
    if (mode != VIZ_MONITOR && mode != VIZ_DECOR) return;
    atomic_store_explicit(&s_viz_mode, (int)mode, memory_order_release);
}

void audio_viz_snapshot_get(audio_viz_snapshot_t *out)
{
    if (!out) return;

    for (;;) {
        int idx = atomic_load_explicit(&s_viz_ready, memory_order_acquire);
        unsigned before = atomic_load_explicit(&s_viz_seq[idx], memory_order_acquire);
        if (before & 1U) continue;                  /* producer owns this slot */

        memcpy(out, &s_viz[idx], sizeof(*out));
        atomic_thread_fence(memory_order_acquire);

        unsigned after = atomic_load_explicit(&s_viz_seq[idx], memory_order_acquire);
        if (before == after && !(after & 1U)) return;
    }
}

/* ---------- Mute ---------------------------------------------------------- */
static _Atomic int s_mute;

void mute_set(int on)
{
    int value = on ? 1 : 0;
    esp_err_t err = gpio_set_level(MUTE_IO, value);
    if (err == ESP_OK) {
        atomic_store_explicit(&s_mute, value, memory_order_release);
    } else {
        ESP_LOGE(TAG, "mute GPIO update failed: %s", esp_err_to_name(err));
    }
}

int mute_get(void)
{
    return atomic_load_explicit(&s_mute, memory_order_acquire);
}

/* ---------- UI command queue --------------------------------------------- */
typedef enum {
    UI_ACTION_EVENT,
    UI_ACTION_SCENE,
    UI_ACTION_TEMPO,
    UI_ACTION_MUTE_TOGGLE,
} ui_action_type_t;

typedef struct {
    ui_action_type_t type;
    ui_event_t event;
    int content;
    int theme;
    float bpm;
    char renderer_name[16];
} ui_action_t;

static QueueHandle_t     s_ui_queue;
static SemaphoreHandle_t s_ui_ready;

static bool ui_post(const ui_action_t *action)
{
    return s_ui_queue && xQueueSend(s_ui_queue, action, 0) == pdTRUE;
}

bool ui_post_event(ui_event_t event)
{
    const ui_action_t action = { .type = UI_ACTION_EVENT, .event = event };
    return ui_post(&action);
}

bool ui_post_scene(int content, int theme, const char *renderer_name)
{
    ui_action_t action = {
        .type = UI_ACTION_SCENE,
        .content = content,
        .theme = theme,
    };
    if (!renderer_name) return false;
    snprintf(action.renderer_name, sizeof(action.renderer_name), "%s", renderer_name);
    return ui_post(&action);
}

bool ui_post_tempo(float bpm)
{
    if (!isfinite(bpm)) return false;
    const ui_action_t action = { .type = UI_ACTION_TEMPO, .bpm = bpm };
    return ui_post(&action);
}

bool ui_post_mute_toggle(void)
{
    const ui_action_t action = { .type = UI_ACTION_MUTE_TOGGLE };
    return ui_post(&action);
}

/* ---------- I2S / audio task --------------------------------------------- */
static i2s_chan_handle_t s_rx;
static portMUX_TYPE s_rx_overflow_mux = portMUX_INITIALIZER_UNLOCKED;
static unsigned s_rx_overflows;

static bool IRAM_ATTR i2s_rx_overflow_cb(i2s_chan_handle_t handle,
                                         i2s_event_data_t *event,
                                         void *user_ctx)
{
    (void)handle;
    (void)event;
    (void)user_ctx;
    portENTER_CRITICAL_ISR(&s_rx_overflow_mux);
    s_rx_overflows++;
    portEXIT_CRITICAL_ISR(&s_rx_overflow_mux);
    return false;
}

static void audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = DMA_FRAMES;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_MCK,
            .bclk = I2S_BCK,
            .ws = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DIN,
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    const i2s_event_callbacks_t callbacks = {
        .on_recv_q_ovf = i2s_rx_overflow_cb,
    };
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(s_rx, &callbacks, NULL));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
}

static void audio_task(void *arg)
{
    (void)arg;
    static int32_t raw[DMA_FRAMES];
    static float samples[DMA_FRAMES];

    ESP_ERROR_CHECK(fft_map_init());
    tuner_init();
    audio_init();
    music_events_init();

    int producer = 1;
    audio_mode_t active_mode = audio_get_mode();
    viz_mode_t active_viz = (viz_mode_t)atomic_load_explicit(&s_viz_mode, memory_order_acquire);
    fft_map_set_mode(active_viz);
    unsigned reported_overflows = 0;

    for (;;) {
        size_t got = 0;
        esp_err_t err = i2s_channel_read(s_rx, raw, sizeof(raw), &got, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (got == 0 || (got % sizeof(raw[0])) != 0) {
            ESP_LOGW(TAG, "I2S returned an invalid byte count: %u", (unsigned)got);
            continue;
        }

        portENTER_CRITICAL(&s_rx_overflow_mux);
        unsigned overflows = s_rx_overflows;
        portEXIT_CRITICAL(&s_rx_overflow_mux);
        if (overflows != reported_overflows) {
            ESP_LOGW(TAG, "I2S RX queue overflow count: %u", overflows);
            reported_overflows = overflows;
        }

        int n = (int)(got / sizeof(raw[0]));
        float sum = 0.0f;
        for (int i = 0; i < n; i++) {
            /* PCM1808's 24 valid bits are left-aligned in this 32-bit slot. */
            float sample = (float)raw[i] / 2147483648.0f;
            samples[i] = sample;
            sum += sample * sample;
        }
        float rms = sqrtf(sum / (float)n);
        float level = rms * 3.0f;
        if (!isfinite(level) || level < 0.0f) level = 0.0f;
        if (level > 1.0f) level = 1.0f;

        audio_mode_t requested_mode = audio_get_mode();
        if (requested_mode != active_mode) {
            active_mode = requested_mode;
            if (active_mode == AUDIO_TUNER) tuner_reset();
            else {
                fft_map_reset();
                publish_empty_viz_frame(&producer);
            }
        }

        viz_mode_t requested_viz =
            (viz_mode_t)atomic_load_explicit(&s_viz_mode, memory_order_acquire);
        if (requested_viz != active_viz) {
            active_viz = requested_viz;
            fft_map_set_mode(active_viz);
        }

        if (active_mode == AUDIO_TUNER) {
            tuner_feed(samples, n);
            music_events_process_block(rms, level);
            continue;
        }

        music_events_process_block(rms, level);

        atomic_fetch_add_explicit(&s_viz_seq[producer], 1U, memory_order_acq_rel);
        int produced = fft_feed(samples, n,
                                s_viz[producer].bars,
                                s_viz[producer].peaks);
        s_viz[producer].level = level;
        atomic_fetch_add_explicit(&s_viz_seq[producer], 1U, memory_order_release);

        if (produced) {
            atomic_store_explicit(&s_viz_ready, producer, memory_order_release);
            producer ^= 1;
        }
    }
}

/* ---------- Core-0 display/input tasks ----------------------------------- */
static void display_task(void *arg)
{
    (void)arg;
    lv_display_t *display = bsp_display_init();
    if (!display) {
        ESP_LOGE(TAG, "display initialization failed");
        vTaskDelete(NULL);
        return;
    }

    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "failed to acquire LVGL lock during startup");
        vTaskDelete(NULL);
        return;
    }
    content_fs_register();
    sm_init();
    lvgl_port_unlock();
    xSemaphoreGive(s_ui_ready);

    for (;;) {
        if (lvgl_port_lock(0)) {
            sm_render();
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

static void dispatch_ui_action(const ui_action_t *action)
{
    if (!lvgl_port_lock(0)) return;
    switch (action->type) {
    case UI_ACTION_EVENT:
        sm_on_event(action->event);
        break;
    case UI_ACTION_SCENE:
        sm_load_scene_named(action->content, action->theme, action->renderer_name);
        break;
    case UI_ACTION_TEMPO:
        sm_set_tempo(action->bpm);
        break;
    case UI_ACTION_MUTE_TOGGLE:
        mute_set(!mute_get());
        break;
    }
    lvgl_port_unlock();
}

typedef enum {
    INPUT_LADDER_NONE = -1,
    INPUT_LADDER_UP,
    INPUT_LADDER_DOWN,
    INPUT_LADDER_LEFT,
    INPUT_LADDER_RIGHT,
    INPUT_LADDER_OK,
    INPUT_LADDER_HOME,
    INPUT_LADDER_COUNT,
} input_ladder_key_t;

#if INPUT_TRS_LADDER
typedef struct {
    float center;
    float window;
    const char *name;
} input_ladder_band_t;

static const input_ladder_band_t s_input_ladder_bands[INPUT_LADDER_COUNT] = {
    [INPUT_LADDER_UP] = {
        .center = INPUT_TRS_UP_CENTER, .window = INPUT_TRS_UP_WINDOW, .name = "UP",
    },
    [INPUT_LADDER_DOWN] = {
        .center = INPUT_TRS_DOWN_CENTER, .window = INPUT_TRS_DOWN_WINDOW, .name = "DOWN",
    },
    [INPUT_LADDER_LEFT] = {
        .center = INPUT_TRS_LEFT_CENTER, .window = INPUT_TRS_LEFT_WINDOW, .name = "LEFT",
    },
    [INPUT_LADDER_RIGHT] = {
        .center = INPUT_TRS_RIGHT_CENTER, .window = INPUT_TRS_RIGHT_WINDOW, .name = "RIGHT",
    },
    [INPUT_LADDER_OK] = {
        .center = INPUT_TRS_OK_CENTER, .window = INPUT_TRS_OK_WINDOW, .name = "OK",
    },
    [INPUT_LADDER_HOME] = {
        .center = INPUT_TRS_HOME_CENTER, .window = INPUT_TRS_HOME_WINDOW, .name = "HOME",
    },
};

static const char *INPUT_TAG = "input";
static adc_oneshot_unit_handle_t s_input_adc;
static adc_cali_handle_t s_input_adc_cali;
static bool s_input_adc_calibrated;
static float s_input_idle_ref_raw;
static float s_input_idle_ref_mv;
static input_ladder_key_t s_input_latched = INPUT_LADDER_NONE;
#if INPUT_TRS_LOG
static TickType_t s_input_last_log;
#endif
static TickType_t s_input_last_error_log;

#if INPUT_TRS_LOG
static const char *input_ladder_key_name(input_ladder_key_t key)
{
    if (key < INPUT_LADDER_UP || key >= INPUT_LADDER_COUNT) return "NONE";
    return s_input_ladder_bands[key].name;
}

static const char *input_ladder_result_name(input_ladder_key_t key, bool idle)
{
    if (idle) return "IDLE";
    if (key == INPUT_LADDER_NONE) return "DEADZONE";
    return input_ladder_key_name(key);
}
#endif

static void input_ladder_init(void)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_input_adc));

    const adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        s_input_adc, ADC_CHANNEL_3, &channel_cfg));

    const adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_3,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t err =
        adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_input_adc_cali);
    s_input_adc_calibrated = (err == ESP_OK);
    if (s_input_adc_calibrated) {
        ESP_LOGI(INPUT_TAG, "TRS ladder ADC calibration: curve fitting enabled");
    } else {
        ESP_LOGW(INPUT_TAG,
                 "TRS ladder ADC calibration unavailable (%s); using raw ratio",
                 esp_err_to_name(err));
    }
}

static esp_err_t input_ladder_read_average(int *raw, int *mv)
{
    int sum = 0;
    for (int i = 0; i < INPUT_TRS_ADC_SAMPLES; i++) {
        int sample = 0;
        esp_err_t err = adc_oneshot_read(s_input_adc, ADC_CHANNEL_3, &sample);
        if (err != ESP_OK) return err;
        sum += sample;
    }

    *raw = (sum + INPUT_TRS_ADC_SAMPLES / 2) / INPUT_TRS_ADC_SAMPLES;
    *mv = -1;
    if (s_input_adc_calibrated) {
        (void)adc_cali_raw_to_voltage(s_input_adc_cali, *raw, mv);
    }
    return ESP_OK;
}

static input_ladder_key_t input_ladder_decode(float ratio, bool *idle)
{
    *idle = ratio >= INPUT_TRS_IDLE_MIN;
    if (*idle) return INPUT_LADDER_NONE;

    for (int key = INPUT_LADDER_UP; key < INPUT_LADDER_COUNT; key++) {
        const input_ladder_band_t *band = &s_input_ladder_bands[key];
        if (fabsf(ratio - band->center) <= band->window) {
            return (input_ladder_key_t)key;
        }
    }
    return INPUT_LADDER_NONE;
}

static void input_ladder_log(int raw, int mv, float ratio,
                             input_ladder_key_t decoded, bool idle)
{
#if INPUT_TRS_LOG
    TickType_t now = xTaskGetTickCount();
    if ((now - s_input_last_log) < pdMS_TO_TICKS(1000)) return;
    s_input_last_log = now;

    if (mv >= 0 && s_input_idle_ref_mv > 0.0f) {
        ESP_LOGI(INPUT_TAG,
                 "ladder raw=%d mV=%d ratio=%.4f -> %s "
                 "(latch=%s idle_ref=%dmV)",
                 raw, mv, (double)ratio,
                 input_ladder_result_name(decoded, idle),
                 input_ladder_key_name(s_input_latched),
                 (int)(s_input_idle_ref_mv + 0.5f));
    } else {
        ESP_LOGI(INPUT_TAG,
                 "ladder raw=%d mV=n/a ratio=%.4f -> %s "
                 "(latch=%s idle_ref_raw=%.0f)",
                 raw, (double)ratio,
                 input_ladder_result_name(decoded, idle),
                 input_ladder_key_name(s_input_latched),
                 (double)s_input_idle_ref_raw);
    }
#else
    (void)raw;
    (void)mv;
    (void)ratio;
    (void)decoded;
    (void)idle;
#endif
}

static void input_ladder_poll(void)
{
    int raw = 0;
    int mv = -1;
    esp_err_t err = input_ladder_read_average(&raw, &mv);
    if (err != ESP_OK) {
        TickType_t now = xTaskGetTickCount();
        if ((now - s_input_last_error_log) >= pdMS_TO_TICKS(1000)) {
            s_input_last_error_log = now;
            ESP_LOGW(INPUT_TAG, "TRS ladder ADC read failed: %s",
                     esp_err_to_name(err));
        }
        return;
    }

    if (s_input_idle_ref_raw <= 0.0f) s_input_idle_ref_raw = (float)raw;
    if (mv >= 0 && s_input_idle_ref_mv <= 0.0f) s_input_idle_ref_mv = (float)mv;
    float ratio;
    if (mv >= 0 && s_input_idle_ref_mv > 0.0f) {
        ratio = (float)mv / s_input_idle_ref_mv;
    } else {
        ratio = s_input_idle_ref_raw > 0.0f
                    ? (float)raw / s_input_idle_ref_raw
                    : 1.0f;
    }
    bool idle = false;
    input_ladder_key_t decoded = input_ladder_decode(ratio, &idle);

    if (s_input_latched == INPUT_LADDER_NONE) {
        if (decoded != INPUT_LADDER_NONE) s_input_latched = decoded;
    } else if (idle) {
        s_input_latched = INPUT_LADDER_NONE;
    } else if (decoded != INPUT_LADDER_NONE &&
               decoded != s_input_latched &&
               s_input_ladder_bands[decoded].center >
                   s_input_ladder_bands[s_input_latched].center) {
        s_input_latched = decoded;
    }

    if (idle) {
        s_input_idle_ref_raw =
            s_input_idle_ref_raw * (1.0f - INPUT_TRS_IDLE_TRACK) +
            (float)raw * INPUT_TRS_IDLE_TRACK;
        if (mv >= 0) {
            s_input_idle_ref_mv =
                s_input_idle_ref_mv * (1.0f - INPUT_TRS_IDLE_TRACK) +
                (float)mv * INPUT_TRS_IDLE_TRACK;
        }
    }
    input_ladder_log(raw, mv, ratio, decoded, idle);
}
#endif

typedef struct {
    gpio_num_t pin;
    input_ladder_key_t ladder_key;
    ui_event_t ev_short;
    ui_event_t ev_hold;
    bool has_hold;
    bool repeats;
    int raw_level;
    int stable_level;
    int debounce_ms;
    int held_ms;
    int repeat_ms;
    bool hold_fired;
} input_button_t;

static int input_button_read_raw(const input_button_t *button)
{
#if INPUT_TRS_LADDER
    if (button->ladder_key != INPUT_LADDER_NONE) {
        return button->ladder_key == s_input_latched ? 0 : 1;
    }
#endif
    return gpio_get_level(button->pin);
}

static void dispatch_input_event(ui_event_t event)
{
    const ui_action_t action = { .type = UI_ACTION_EVENT, .event = event };
    dispatch_ui_action(&action);
}

static void input_button_pressed(input_button_t *button)
{
    button->held_ms = 0;
    button->repeat_ms = 0;
    button->hold_fired = false;
    if (!button->has_hold) dispatch_input_event(button->ev_short);
}

static void input_button_released(input_button_t *button)
{
    if (button->has_hold && !button->hold_fired) {
        dispatch_input_event(button->ev_short);
    }
    button->held_ms = 0;
    button->repeat_ms = 0;
    button->hold_fired = false;
}

static void input_button_update(input_button_t *button)
{
    int raw = input_button_read_raw(button);
    if (raw != button->raw_level) {
        button->raw_level = raw;
        button->debounce_ms = 0;
    } else if (button->debounce_ms < INPUT_DEBOUNCE_MS) {
        button->debounce_ms += INPUT_POLL_MS;
        if (button->debounce_ms >= INPUT_DEBOUNCE_MS &&
            button->stable_level != button->raw_level) {
            button->stable_level = button->raw_level;
            if (button->stable_level == 0) input_button_pressed(button);
            else input_button_released(button);
        }
    }

    if (button->raw_level != button->stable_level) return;
    if (button->stable_level != 0) return;

    button->held_ms += INPUT_POLL_MS;
    if (button->has_hold && !button->hold_fired &&
        button->held_ms >= INPUT_HOLD_MS) {
        button->hold_fired = true;
        dispatch_input_event(button->ev_hold);
    }

    if (button->repeats && button->held_ms >= INPUT_REPEAT_DELAY_MS) {
        button->repeat_ms += INPUT_POLL_MS;
        if (button->repeat_ms >= INPUT_REPEAT_RATE_MS) {
            button->repeat_ms = 0;
            dispatch_input_event(button->ev_short);
        }
    }
}

static void input_task(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_ui_ready, portMAX_DELAY);

    input_button_t buttons[] = {
        { .pin = BTN_UP_IO, .ladder_key = INPUT_LADDER_UP,
          .ev_short = EV_UP, .repeats = true },
        { .pin = BTN_DOWN_IO, .ladder_key = INPUT_LADDER_DOWN,
          .ev_short = EV_DOWN, .repeats = true },
        { .pin = BTN_LEFT_IO, .ladder_key = INPUT_LADDER_LEFT,
          .ev_short = EV_LEFT, .repeats = true },
        { .pin = BTN_RIGHT_IO, .ladder_key = INPUT_LADDER_RIGHT,
          .ev_short = EV_RIGHT, .repeats = true },
        { .pin = BTN_OK_IO, .ladder_key = INPUT_LADDER_OK,
          .ev_short = EV_OK },
        { .pin = BTN_HOME_IO, .ladder_key = INPUT_LADDER_HOME,
          .ev_short = EV_HOME,
          .ev_hold = EV_HOME_HOLD, .has_hold = true },
        { .pin = FOOTSW_IO, .ladder_key = INPUT_LADDER_NONE,
          .ev_short = EV_FOOTSW,
          .ev_hold = EV_FOOTSW_HOLD, .has_hold = true },
    };
    const int button_count = (int)(sizeof(buttons) / sizeof(buttons[0]));
    const gpio_config_t input_cfg = {
#if INPUT_TRS_LADDER
        .pin_bit_mask = (1ULL << FOOTSW_IO),
#else
        .pin_bit_mask = (1ULL << BTN_UP_IO) | (1ULL << BTN_DOWN_IO) |
                        (1ULL << BTN_LEFT_IO) | (1ULL << BTN_RIGHT_IO) |
                        (1ULL << BTN_OK_IO) | (1ULL << BTN_HOME_IO) |
                        (1ULL << FOOTSW_IO),
#endif
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_cfg));

#if INPUT_TRS_LADDER
    input_ladder_init();
    input_ladder_poll();
#endif
    for (int i = 0; i < button_count; i++) {
        int level = input_button_read_raw(&buttons[i]);
        buttons[i].raw_level = level;
        buttons[i].stable_level = level;
        buttons[i].debounce_ms = INPUT_DEBOUNCE_MS;
    }

    for (;;) {
        ui_action_t queued;
        while (xQueueReceive(s_ui_queue, &queued, 0) == pdTRUE) {
            dispatch_ui_action(&queued);
        }

#if INPUT_TRS_LADDER
        input_ladder_poll();
#endif
        for (int i = 0; i < button_count; i++) input_button_update(&buttons[i]);
        vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_MS));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(gpio_set_direction(MUTE_IO, GPIO_MODE_OUTPUT));
    mute_set(0);

    s_ui_queue = xQueueCreate(16, sizeof(ui_action_t));
    s_ui_ready = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK((s_ui_queue && s_ui_ready) ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 6, NULL, 1) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 4, NULL, 0) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(input_task, "input", 4096, NULL, 5, NULL, 0) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(TAG, "boot complete");
}
