#pragma once

#include <stdint.h>

#include "gadget_app.h"

#define APP_SLOT_MAX 32

typedef enum {
    CHAIN_LIVE = 0,
    CHAIN_STASH = 1,
} chain_t;

typedef struct {
    const gadget_app_t *app;
    chain_t chain;
    uint8_t order;
    uint8_t variant;
    uint8_t local_theme;
} app_slot_t;

void app_slots_init(void);
void app_slots_save(void);
int app_slots_count(void);
app_slot_t *app_slots_at(int idx);
const gadget_app_t *app_slots_first_live(void);
const gadget_app_t *app_slots_next_live(const gadget_app_t *cur);
const gadget_app_t *app_slots_prev_live(const gadget_app_t *cur);
const char *app_slots_last_view(void);
void app_slots_set_last_view(const char *id);
const char *app_slots_quick_app(void);
uint8_t app_slots_theme(void);
void app_slots_set_theme(uint8_t idx);
uint8_t app_slots_local_theme(const gadget_app_t *app);
void app_slots_set_local_theme(const gadget_app_t *app, uint8_t idx);
void app_slots_set_local_theme_runtime(const gadget_app_t *app, uint8_t idx);
