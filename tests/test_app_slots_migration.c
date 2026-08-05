#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_slots.h"
#include "platform.h"

#define TEST_APP_CFG_MAGIC 0x6741u
#define TEST_APP_CFG_V5 5u
#define TEST_APP_CFG_V6 6u
#define TEST_APP_ID_LEN 16

typedef struct {
    chain_t chain;
    uint8_t order;
    uint8_t variant;
    uint8_t appearance;
    uint8_t options;
} saved_app_setting_t;

typedef struct {
    char id[TEST_APP_ID_LEN];
    saved_app_setting_t s;
} saved_app_entry_t;

typedef struct {
    uint16_t magic;
    uint16_t version;
    char last_view_id[TEST_APP_ID_LEN];
    char quick_app_id[TEST_APP_ID_LEN];
    uint8_t theme_idx;
    uint8_t reserved[7];
    saved_app_entry_t apps[APP_SLOT_MAX];
} saved_platform_config_t;

_Static_assert(sizeof(saved_app_setting_t) == 8,
               "test must match the persisted app setting layout");

static saved_platform_config_t s_loaded;
static saved_platform_config_t s_saved;
static bool s_saved_called;

static int monitor_mode_count(void)
{
    return 4;
}

static int single_mode_count(void)
{
    return 1;
}

static const gadget_app_t TEST_APPS[] = {
    {
        .id = "monitor",
        .name = "Sound Monitor",
        .mode_count = monitor_mode_count,
    },
    {
        .id = "other",
        .name = "Other",
        .mode_count = single_mode_count,
    },
};

int app_registry_count(void)
{
    return (int)(sizeof(TEST_APPS) / sizeof(TEST_APPS[0]));
}

const gadget_app_t *app_registry_at(int idx)
{
    return idx >= 0 && idx < app_registry_count() ? &TEST_APPS[idx] : NULL;
}

int app_registry_find(const char *id)
{
    for (int i = 0; i < app_registry_count(); i++) {
        if (id && strcmp(TEST_APPS[i].id, id) == 0) return i;
    }
    return -1;
}

int theme_index_for(ui_theme_mode_t mode, ui_theme_color_t color)
{
    return (int)mode * UI_THEME_COLOR_COUNT + (int)color;
}

void plat_nvs_load(void *blob, size_t n, bool *found)
{
    if (found) *found = false;
    if (!blob || n != sizeof(s_loaded)) return;
    memcpy(blob, &s_loaded, n);
    if (found) *found = true;
}

void plat_nvs_save(const void *blob, size_t n)
{
    if (!blob || n != sizeof(s_saved)) return;
    memcpy(&s_saved, blob, n);
    s_saved_called = true;
}

static void set_id(char dst[TEST_APP_ID_LEN], const char *src)
{
    strncpy(dst, src, TEST_APP_ID_LEN - 1);
    dst[TEST_APP_ID_LEN - 1] = 0;
}

static int require(bool condition, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    memset(&s_loaded, 0, sizeof(s_loaded));
    s_loaded.magic = TEST_APP_CFG_MAGIC;
    s_loaded.version = TEST_APP_CFG_V5;
    s_loaded.theme_idx = 1; /* Legacy White. */
    set_id(s_loaded.quick_app_id, "monitor");

    set_id(s_loaded.apps[0].id, "monitor");
    s_loaded.apps[0].s.chain = CHAIN_LIVE;
    s_loaded.apps[0].s.appearance = 2u | (3u << 2); /* White + Reference. */

    set_id(s_loaded.apps[1].id, "other");
    s_loaded.apps[1].s.chain = CHAIN_STASH;
    s_loaded.apps[1].s.order = 1;
    s_loaded.apps[1].s.appearance = 3u; /* Legacy Green + mode 0. */

    app_slots_init();

    if (require(s_saved_called, "migration did not persist the v6 blob") ||
        require(s_saved.version == TEST_APP_CFG_V6,
                "saved schema is not v6") ||
        require(app_slots_theme() ==
                    theme_index_for(UI_THEME_MODE_LIGHT,
                                    UI_THEME_COLOR_BLUE),
                "legacy White global theme did not become Light + Blue") ||
        require(app_slots_color(&TEST_APPS[0]) == APP_COLOR_BLUE,
                "legacy app White did not become Blue") ||
        require(app_slots_mode(&TEST_APPS[0]) == 3,
                "legacy monitor mode was not preserved") ||
        require(app_slots_color(&TEST_APPS[1]) == APP_COLOR_GREEN,
                "legacy app Green was not preserved") ||
        require(s_saved.apps[0].s.appearance == (1u | (3u << 3)),
                "monitor appearance was not repacked as 3+5 bits") ||
        require(s_saved.apps[1].s.appearance == 2u,
                "Green appearance was not repacked")) {
        return 1;
    }

    printf("app slot v5 -> v6 migration: PASS\n");
    return 0;
}
