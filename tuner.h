#pragma once
typedef struct {
    int voiced; float f0; const char* name; int octave; float cents; float clarity;
} tuner_result_t;
void tuner_init(void);
void tuner_reset(void);
int  tuner_feed(const float* samples, int n);
void tuner_get(tuner_result_t* out);
