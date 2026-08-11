#pragma once

#include <stdbool.h>

#include "app.h"

bool plat_sim_configure(int argc, char **argv);
bool plat_sim_should_exit_after_args(void);
bool plat_sim_should_quit(void);
bool plat_sim_is_smoke_test(void);
bool plat_sim_is_renderer_benchmark(void);
const char *plat_sim_preview(void);
bool plat_sim_post_event(ui_event_t event);
void plat_sim_trigger_onset(void);
bool plat_sim_midi_inject_short(uint8_t status, uint8_t data1, uint8_t data2);
