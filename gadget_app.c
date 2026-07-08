#include "gadget_app.h"

#include <string.h>

#define MAX_APPS 10

static const gadget_app_t *s_apps[MAX_APPS];
static int s_app_count;
static int s_initialized;

void app_registry_register(const gadget_app_t *app)
{
    if (!app || !app->id || !app->name || s_app_count >= MAX_APPS) return;
    if (app_registry_find(app->id) >= 0) return;
    s_apps[s_app_count++] = app;
}

int app_registry_count(void)
{
    return s_app_count;
}

const gadget_app_t *app_registry_at(int idx)
{
    return (idx >= 0 && idx < s_app_count) ? s_apps[idx] : 0;
}

int app_registry_find(const char *id)
{
    if (!id) return -1;
    for (int i = 0; i < s_app_count; i++) {
        if (!strcmp(s_apps[i]->id, id)) return i;
    }
    return -1;
}

const char *app_registry_name(int idx)
{
    const gadget_app_t *app = app_registry_at(idx);
    return app ? app->name : "";
}

void apps_init(void)
{
    if (s_initialized) return;
    s_initialized = 1;

    app_registry_register(&APP_MONITOR);
    app_registry_register(&APP_IMAGES);
    app_registry_register(&APP_TUNER);
    app_registry_register(&APP_BOUNCE);
}
