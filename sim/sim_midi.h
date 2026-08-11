#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

bool sim_midi_init(int argc, char **argv, bool virtual_ports);
void sim_midi_shutdown(void);
void sim_midi_pump(void);
bool sim_midi_should_exit_after_args(void);
void sim_midi_get_status(platform_midi_status_t *out);
bool sim_midi_send_short(uint8_t status, uint8_t data1, uint8_t data2);
bool sim_midi_inject_short(uint8_t status, uint8_t data1, uint8_t data2);
