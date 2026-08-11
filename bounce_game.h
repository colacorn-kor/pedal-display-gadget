#pragma once

#include <stdbool.h>

#include "gadget_app.h"

void bounce_game_enter(const gadget_app_t *appearance_owner,
                       const char *title);
void bounce_game_exit(void);
void bounce_game_render(void);
bool bounce_game_on_event(ui_event_t event);
void bounce_game_apply_appearance(void);
