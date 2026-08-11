#include "platform.h"

#include <stdatomic.h>
#include <string.h>

#include "audio_playback.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#define PLAT_NVS_NAMESPACE "gadget"
#define PLAT_NVS_KEY "cfg"

static const char *TAG = "platform";
static bool s_nvs_checked;
static bool s_nvs_ready;
static atomic_uint s_game_input_state;

static bool nvs_ready(void)
{
    if (s_nvs_checked) return s_nvs_ready;
    s_nvs_checked = true;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs reset: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "NVS erase failed: %s", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }

    s_nvs_ready = true;
    return true;
}

void plat_init(void)
{
    atomic_init(&s_game_input_state, 0u);
    (void)audio_playback_init(false);
}

platform_capability_mask_t plat_capabilities(void)
{
    return PLAT_CAP_DISPLAY |
           PLAT_CAP_AUDIO_ANALYSIS_INPUT |
           PLAT_CAP_MEDIA_STORAGE |
           PLAT_CAP_GAME_RUNTIME;
}

uint32_t plat_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint64_t plat_micros(void)
{
    return (uint64_t)esp_timer_get_time();
}

bool plat_input_poll(ui_event_t *ev)
{
    (void)ev;
    return false;
}

platform_game_input_mask_t plat_game_input_state(void)
{
    return atomic_load_explicit(&s_game_input_state, memory_order_acquire);
}

void plat_game_input_publish(platform_game_input_mask_t held_mask)
{
    atomic_store_explicit(
        &s_game_input_state, held_mask, memory_order_release);
}

void plat_nvs_load(void *blob, size_t n, bool *found)
{
    if (found) *found = false;
    if (!blob || n == 0 || !nvs_ready()) return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(PLAT_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return;

    size_t size = 0;
    err = nvs_get_blob(handle, PLAT_NVS_KEY, NULL, &size);
    if (err != ESP_OK || size != n) {
        nvs_close(handle);
        return;
    }

    err = nvs_get_blob(handle, PLAT_NVS_KEY, blob, &size);
    nvs_close(handle);
    if (err == ESP_OK && found) *found = true;
}

void plat_nvs_save(const void *blob, size_t n)
{
    if (!blob || n == 0 || !nvs_ready()) return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(PLAT_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open for save failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, PLAT_NVS_KEY, blob, n);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS save failed: %s", esp_err_to_name(err));
    }
}

void plat_audio_viz_get(audio_viz_snapshot_t *out)
{
    audio_viz_snapshot_get(out);
}

void plat_music_get(music_snapshot_t *out)
{
    music_snapshot_get(out);
}

void plat_midi_get_status(platform_midi_status_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
}

bool plat_midi_send_short(uint8_t status, uint8_t data1, uint8_t data2)
{
    (void)status;
    (void)data1;
    (void)data2;
    return false;
}

void plat_lvgl_lock(void)
{
    (void)lvgl_port_lock(0);
}

void plat_lvgl_unlock(void)
{
    lvgl_port_unlock();
}
