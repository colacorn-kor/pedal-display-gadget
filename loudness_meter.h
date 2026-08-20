#pragma once

#include <stdint.h>

#define LOUDNESS_FLOOR_LUFS (-120.0f)

typedef struct {
    float momentary_lufs;
    float short_term_lufs;
    float integrated_lufs;
    float true_peak_dbtp;
    float true_peak_linear;
    float relative_gate_lufs;
    uint32_t integrated_blocks;
    uint32_t elapsed_ms;
} loudness_snapshot_t;

void loudness_meter_init(void);
void loudness_meter_reset(void);
void loudness_meter_feed(const float *samples, int count);
void loudness_meter_get(loudness_snapshot_t *out);
