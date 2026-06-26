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
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "app.h"
#include "content_screen.h"
#include "display_bringup.h"
#include "fft_map.h"
#include "tuner.h"

#define DMA_FRAMES  256
#define MUTE_IO     GPIO_NUM_21
#define BTN_UP_IO   GPIO_NUM_1
#define BTN_DOWN_IO GPIO_NUM_2
#define BTN_LEFT_IO GPIO_NUM_4
#define BTN_OK_IO   GPIO_NUM_5
#define BTN_RIGHT_IO GPIO_NUM_6
#define BTN_HOME_IO GPIO_NUM_13
#define FOOTSW_IO   GPIO_NUM_7
#define I2S_MCK     GPIO_NUM_16
#define I2S_BCK     GPIO_NUM_17
#define I2S_WS      GPIO_NUM_18
#define I2S_DIN     GPIO_NUM_15

#define INPUT_POLL_MS         10
#define INPUT_DEBOUNCE_MS     30
#define INPUT_HOLD_MS         500
#define INPUT_REPEAT_DELAY_MS 400
#define INPUT_REPEAT_RATE_MS  120

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
        float level = sqrtf(sum / (float)n) * 3.0f;
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
            continue;
        }

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

typedef struct {
    gpio_num_t pin;
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
    int raw = gpio_get_level(button->pin);
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
        { .pin = BTN_UP_IO, .ev_short = EV_UP, .repeats = true },
        { .pin = BTN_DOWN_IO, .ev_short = EV_DOWN, .repeats = true },
        { .pin = BTN_LEFT_IO, .ev_short = EV_LEFT, .repeats = true },
        { .pin = BTN_RIGHT_IO, .ev_short = EV_RIGHT, .repeats = true },
        { .pin = BTN_OK_IO, .ev_short = EV_OK },
        { .pin = BTN_HOME_IO, .ev_short = EV_HOME,
          .ev_hold = EV_HOME_HOLD, .has_hold = true },
        { .pin = FOOTSW_IO, .ev_short = EV_FOOTSW,
          .ev_hold = EV_FOOTSW_HOLD, .has_hold = true },
    };
    const int button_count = (int)(sizeof(buttons) / sizeof(buttons[0]));
    const gpio_config_t input_cfg = {
        .pin_bit_mask = (1ULL << BTN_UP_IO) | (1ULL << BTN_DOWN_IO) |
                        (1ULL << BTN_LEFT_IO) | (1ULL << BTN_RIGHT_IO) |
                        (1ULL << BTN_OK_IO) | (1ULL << BTN_HOME_IO) |
                        (1ULL << FOOTSW_IO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_cfg));

    for (int i = 0; i < button_count; i++) {
        int level = gpio_get_level(buttons[i].pin);
        buttons[i].raw_level = level;
        buttons[i].stable_level = level;
        buttons[i].debounce_ms = INPUT_DEBOUNCE_MS;
    }

    for (;;) {
        ui_action_t queued;
        while (xQueueReceive(s_ui_queue, &queued, 0) == pdTRUE) {
            dispatch_ui_action(&queued);
        }

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
