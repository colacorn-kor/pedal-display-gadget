#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    WAV_DECODER_OK = 0,
    WAV_DECODER_ERR_IO,
    WAV_DECODER_ERR_CONTAINER,
    WAV_DECODER_ERR_FORMAT,
    WAV_DECODER_ERR_EMPTY,
    WAV_DECODER_ERR_TRUNCATED,
} wav_decoder_error_t;

typedef struct {
    FILE *file;
    uint32_t sample_rate;
    uint32_t data_remaining;
    uint32_t total_source_frames;
    uint64_t total_output_frames;
    uint64_t output_frames;
    uint16_t format;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint16_t block_align;
    double phase;
    double source_step;
    float frame_a[2];
    float frame_b[2];
    bool last_frame;
    bool finished;
    wav_decoder_error_t error;
} wav_decoder_t;

/* The decoder owns file after this call, including when parsing fails. */
bool wav_decoder_open(wav_decoder_t *decoder, FILE *file);
void wav_decoder_close(wav_decoder_t *decoder);
size_t wav_decoder_read(wav_decoder_t *decoder,
                        float *stereo_interleaved,
                        size_t output_frames);
bool wav_decoder_finished(const wav_decoder_t *decoder);
uint32_t wav_decoder_position_ms(const wav_decoder_t *decoder);
uint32_t wav_decoder_duration_ms(const wav_decoder_t *decoder);
const char *wav_decoder_error_text(wav_decoder_error_t error);
