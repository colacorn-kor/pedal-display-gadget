#include "app_slots.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define APP_CFG_MAGIC 0x6741u
#define APP_CFG_VERSION 1u
#define APP_ID_LEN 16
#define APP_CFG_NAMESPACE "gadget"
#define APP_CFG_KEY "cfg"
#define APP_QUICK_DEFAULT "tuner"

typedef struct {
    chain_t chain;
    uint8_t order;
    uint8_t variant;
} app_setting_t;

typedef struct {
    char id[APP_ID_LEN];
    app_setting_t s;
} app_cfg_entry_t;

typedef struct {
    uint16_t magic;
    uint16_t version;
    char last_view_id[APP_ID_LEN];
    char quick_app_id[APP_ID_LEN];
    app_cfg_entry_t apps[APP_SLOT_MAX];
} platform_config_t;

static const char *TAG = "app_slots";

static app_slot_t s_slots[APP_SLOT_MAX];
static int s_slot_count;
static platform_config_t s_cfg;
static bool s_initialized;
static bool s_nvs_ready;

static void copy_id(char dst[APP_ID_LEN], const char *src)
{
    if (!src) src = "";
    strncpy(dst, src, APP_ID_LEN - 1);
    dst[APP_ID_LEN - 1] = 0;
}

static bool same_id(const char saved[APP_ID_LEN], const char *id)
{
    return id && strncmp(saved, id, APP_ID_LEN) == 0;
}

static uint8_t max_variant_for_app(const gadget_app_t *app)
{
    if (!app || app->variant_count <= 1) return 0;
    if (app->variant_count > 256) return 255;
    return (uint8_t)(app->variant_count - 1);
}

static uint8_t clamp_variant(const gadget_app_t *app, uint8_t variant,
                             bool *changed)
{
    const uint8_t max_variant = max_variant_for_app(app);
    if (variant <= max_variant) return variant;
    if (changed) *changed = true;
    return max_variant;
}

static void default_config(platform_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = APP_CFG_MAGIC;
    cfg->version = APP_CFG_VERSION;
    copy_id(cfg->quick_app_id, APP_QUICK_DEFAULT);

    int n = app_registry_count();
    if (n > APP_SLOT_MAX) n = APP_SLOT_MAX;
    for (int i = 0; i < n; i++) {
        const gadget_app_t *app = app_registry_at(i);
        if (!app) continue;
        copy_id(cfg->apps[i].id, app->id);
        cfg->apps[i].s.chain = CHAIN_LIVE;
        cfg->apps[i].s.order = (uint8_t)i;
        cfg->apps[i].s.variant = 0;
    }
}

static void terminate_config_strings(platform_config_t *cfg)
{
    cfg->last_view_id[APP_ID_LEN - 1] = 0;
    cfg->quick_app_id[APP_ID_LEN - 1] = 0;
    for (int i = 0; i < APP_SLOT_MAX; i++) {
        cfg->apps[i].id[APP_ID_LEN - 1] = 0;
    }
}

static esp_err_t init_nvs_once(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs reset: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "NVS erase failed: %s", esp_err_to_name(err));
            return err;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t read_config(platform_config_t *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_CFG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t size = 0;
    err = nvs_get_blob(handle, APP_CFG_KEY, NULL, &size);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    if (size != sizeof(*cfg)) {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }

    err = nvs_get_blob(handle, APP_CFG_KEY, cfg, &size);
    nvs_close(handle);
    return err;
}

static int find_cfg_entry(const platform_config_t *cfg, const char *id)
{
    for (int i = 0; i < APP_SLOT_MAX; i++) {
        if (cfg->apps[i].id[0] && same_id(cfg->apps[i].id, id)) return i;
    }
    return -1;
}

static bool cfg_has_unknown_or_duplicate_entries(const platform_config_t *cfg)
{
    bool changed = false;
    for (int i = 0; i < APP_SLOT_MAX; i++) {
        if (!cfg->apps[i].id[0]) continue;
        if (app_registry_find(cfg->apps[i].id) < 0) {
            ESP_LOGW(TAG, "Ignoring saved app id '%s'", cfg->apps[i].id);
            changed = true;
            continue;
        }
        for (int j = 0; j < i; j++) {
            if (cfg->apps[j].id[0] &&
                strncmp(cfg->apps[j].id, cfg->apps[i].id, APP_ID_LEN) == 0) {
                ESP_LOGW(TAG, "Ignoring duplicate saved app id '%s'",
                         cfg->apps[i].id);
                changed = true;
                break;
            }
        }
    }
    return changed;
}

static bool bind_slots_from_config(void)
{
    bool changed = cfg_has_unknown_or_duplicate_entries(&s_cfg);

    memset(s_slots, 0, sizeof(s_slots));
    s_slot_count = 0;

    int n = app_registry_count();
    if (n > APP_SLOT_MAX) {
        ESP_LOGW(TAG, "Registry has %d apps; only %d slots are supported",
                 n, APP_SLOT_MAX);
        n = APP_SLOT_MAX;
        changed = true;
    }

    for (int i = 0; i < n; i++) {
        const gadget_app_t *app = app_registry_at(i);
        if (!app) continue;

        app_slot_t *slot = &s_slots[s_slot_count];
        slot->app = app;
        slot->chain = CHAIN_LIVE;
        slot->order = (uint8_t)i;
        slot->variant = 0;

        const int cfg_idx = find_cfg_entry(&s_cfg, app->id);
        if (cfg_idx >= 0) {
            const app_setting_t *setting = &s_cfg.apps[cfg_idx].s;
            slot->chain = setting->chain;
            slot->order = setting->order;
            slot->variant = setting->variant;
        } else {
            ESP_LOGW(TAG, "Adding new app slot for '%s'", app->id);
            changed = true;
        }

        if (slot->chain != CHAIN_LIVE && slot->chain != CHAIN_STASH) {
            ESP_LOGW(TAG, "Invalid chain for '%s'; using live", app->id);
            slot->chain = CHAIN_LIVE;
            changed = true;
        }

        slot->variant = clamp_variant(app, slot->variant, &changed);
        s_slot_count++;
    }

    if (!s_cfg.quick_app_id[0]) {
        copy_id(s_cfg.quick_app_id, APP_QUICK_DEFAULT);
        changed = true;
    } else if (app_registry_find(s_cfg.quick_app_id) < 0) {
        ESP_LOGW(TAG, "Invalid quick app id '%s'; using '%s'",
                 s_cfg.quick_app_id, APP_QUICK_DEFAULT);
        copy_id(s_cfg.quick_app_id, APP_QUICK_DEFAULT);
        changed = true;
    }

    return changed;
}

static int find_slot_for_app(const gadget_app_t *app)
{
    if (!app) return -1;
    for (int i = 0; i < s_slot_count; i++) {
        if (s_slots[i].app == app) return i;
    }
    return -1;
}

static bool slot_is_live(int idx)
{
    return idx >= 0 && idx < s_slot_count &&
           s_slots[idx].app && s_slots[idx].chain == CHAIN_LIVE;
}

static bool slot_before(int a, int b)
{
    if (b < 0) return true;
    if (s_slots[a].order != s_slots[b].order) {
        return s_slots[a].order < s_slots[b].order;
    }
    return a < b;
}

static bool slot_after(int a, int b)
{
    return slot_before(b, a);
}

static int first_live_slot(void)
{
    int best = -1;
    for (int i = 0; i < s_slot_count; i++) {
        if (slot_is_live(i) && slot_before(i, best)) best = i;
    }
    return best;
}

static int last_live_slot(void)
{
    int best = -1;
    for (int i = 0; i < s_slot_count; i++) {
        if (slot_is_live(i) && (best < 0 || slot_after(i, best))) best = i;
    }
    return best;
}

static int next_live_slot(int cur)
{
    if (!slot_is_live(cur)) return first_live_slot();

    int best_after = -1;
    for (int i = 0; i < s_slot_count; i++) {
        if (!slot_is_live(i) || !slot_after(i, cur)) continue;
        if (slot_before(i, best_after)) best_after = i;
    }
    return best_after >= 0 ? best_after : first_live_slot();
}

static int prev_live_slot(int cur)
{
    if (!slot_is_live(cur)) return first_live_slot();

    int best_before = -1;
    for (int i = 0; i < s_slot_count; i++) {
        if (!slot_is_live(i) || !slot_before(i, cur)) continue;
        if (best_before < 0 || slot_after(i, best_before)) best_before = i;
    }
    return best_before >= 0 ? best_before : last_live_slot();
}

void app_slots_init(void)
{
    if (s_initialized) return;
    s_initialized = true;

    default_config(&s_cfg);

    s_nvs_ready = init_nvs_once() == ESP_OK;
    bool needs_save = !s_nvs_ready;
    if (s_nvs_ready) {
        platform_config_t loaded;
        esp_err_t err = read_config(&loaded);
        if (err == ESP_OK &&
            loaded.magic == APP_CFG_MAGIC &&
            loaded.version == APP_CFG_VERSION) {
            s_cfg = loaded;
            terminate_config_strings(&s_cfg);
        } else {
            ESP_LOGW(TAG, "Using default config: %s",
                     err == ESP_OK ? "schema mismatch" : esp_err_to_name(err));
            needs_save = true;
        }
    }

    if (bind_slots_from_config()) needs_save = true;
    if (needs_save) app_slots_save();
}

void app_slots_save(void)
{
    if (!s_nvs_ready) return;

    s_cfg.magic = APP_CFG_MAGIC;
    s_cfg.version = APP_CFG_VERSION;
    for (int i = 0; i < APP_SLOT_MAX; i++) {
        memset(&s_cfg.apps[i], 0, sizeof(s_cfg.apps[i]));
    }
    for (int i = 0; i < s_slot_count && i < APP_SLOT_MAX; i++) {
        app_slot_t *slot = &s_slots[i];
        if (!slot->app) continue;
        slot->variant = clamp_variant(slot->app, slot->variant, NULL);
        copy_id(s_cfg.apps[i].id, slot->app->id);
        s_cfg.apps[i].s.chain = slot->chain;
        s_cfg.apps[i].s.order = slot->order;
        s_cfg.apps[i].s.variant = slot->variant;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open for save failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, APP_CFG_KEY, &s_cfg, sizeof(s_cfg));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS save failed: %s", esp_err_to_name(err));
    }
}

int app_slots_count(void)
{
    return s_slot_count;
}

app_slot_t *app_slots_at(int idx)
{
    return (idx >= 0 && idx < s_slot_count) ? &s_slots[idx] : NULL;
}

const gadget_app_t *app_slots_first_live(void)
{
    const int slot = first_live_slot();
    if (slot >= 0) return s_slots[slot].app;
    return app_registry_at(0);
}

const gadget_app_t *app_slots_next_live(const gadget_app_t *cur)
{
    const int slot = next_live_slot(find_slot_for_app(cur));
    if (slot >= 0) return s_slots[slot].app;
    return app_registry_at(0);
}

const gadget_app_t *app_slots_prev_live(const gadget_app_t *cur)
{
    const int slot = prev_live_slot(find_slot_for_app(cur));
    if (slot >= 0) return s_slots[slot].app;
    return app_registry_at(0);
}

const char *app_slots_last_view(void)
{
    return s_cfg.last_view_id;
}

void app_slots_set_last_view(const char *id)
{
    char next[APP_ID_LEN];
    copy_id(next, id);
    if (strncmp(s_cfg.last_view_id, next, APP_ID_LEN) == 0) return;
    copy_id(s_cfg.last_view_id, next);
    app_slots_save();
}

const char *app_slots_quick_app(void)
{
    return s_cfg.quick_app_id[0] ? s_cfg.quick_app_id : APP_QUICK_DEFAULT;
}
