#pragma once
#include "esp_err.h"
typedef enum { VIZ_MONITOR=0, VIZ_DECOR=1 } viz_mode_t;
esp_err_t  fft_map_init(void);
void       fft_map_reset(void);
void       fft_map_set_mode(viz_mode_t m);
viz_mode_t fft_map_get_mode(void);
int        fft_feed(const float* s, int n, float* out, float* peakout);
int        fft_map_num_points(void);
float      fft_map_db_to_norm(float db);
