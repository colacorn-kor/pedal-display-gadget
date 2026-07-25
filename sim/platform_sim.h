#pragma once

#include <stdbool.h>

#include "app.h"

bool plat_sim_configure(int argc, char **argv);
bool plat_sim_should_exit_after_args(void);
bool plat_sim_should_quit(void);
bool plat_sim_is_smoke_test(void);
bool plat_sim_post_event(ui_event_t event);
