#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sim_loopback_sample_fn)(float sample, void *context);

bool sim_loopback_open(char *device_name, size_t device_name_size,
                       int *sample_rate, int *channels);
bool sim_loopback_pump(sim_loopback_sample_fn on_sample, void *context);
void sim_loopback_close(void);

#ifdef __cplusplus
}
#endif
