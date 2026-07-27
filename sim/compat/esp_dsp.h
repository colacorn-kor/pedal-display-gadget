#pragma once

#include "esp_err.h"

esp_err_t dsps_fft2r_init_fc32(float *table, int table_size);
void dsps_wind_hann_f32(float *window, int length);
esp_err_t dsps_fft2r_fc32(float *data, int length);
esp_err_t dsps_bit_rev_fc32(float *data, int length);
