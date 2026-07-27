#pragma once

#include <stdint.h>

/* Shared audio/DSP dimensions. Keep these in one place so producers and
 * renderers cannot silently disagree about buffer sizes. */
#define AUDIO_SAMPLE_RATE 48000
#define VIZ_POINTS        256

/* Shared spectrum display contract. Values may include the active visual
 * profile's display tilt; Reference mode selects 0 dB/oct. */
#define VIZ_FREQ_LO_HZ       20.0f
#define VIZ_FREQ_HI_HZ    20000.0f
#define VIZ_DB_FLOOR        (-72.0f)
#define VIZ_DB_TOP            0.0f
#define VIZ_TILT_PIVOT_HZ  1000.0f
#define VIZ_MONITOR_TILT_DB_OCT 4.5f

/* PCM1808 analog-input nominal full scale. */
#define AUDIO_ADC_FULL_SCALE_VPP   3.0f
#define AUDIO_ADC_FULL_SCALE_VPEAK (AUDIO_ADC_FULL_SCALE_VPP * 0.5f)
#define AUDIO_DBU_REFERENCE_VRMS   0.775f

/* Target Step 5B dual-range input. The production build keeps this disabled
 * until both PCM1808 inputs are wired. HOT defines the fixed GG input scale;
 * SENSITIVE is mapped onto that scale before DSP sees the samples. */
#ifndef AUDIO_DUAL_RANGE
#define AUDIO_DUAL_RANGE 0
#endif
#define AUDIO_DUAL_HOT_GAIN                    (1.5f / 11.5f)
#define AUDIO_DUAL_SENSITIVE_GAIN              3.98f
#ifndef AUDIO_DUAL_HOT_VOLTAGE_CORRECTION
#define AUDIO_DUAL_HOT_VOLTAGE_CORRECTION      1.0f
#endif
#ifndef AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION
#define AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION 1.0f
#endif
#define AUDIO_DUAL_SWITCH_TO_HOT_PEAK           0.82f
#define AUDIO_DUAL_SWITCH_TO_SENSITIVE_PEAK     0.45f
#define AUDIO_DUAL_ADC_CLIP_PEAK                0.985f
#define AUDIO_DUAL_RELEASE_MS                   500U
#define AUDIO_DUAL_RELEASE_SAMPLES \
    ((uint32_t)((AUDIO_SAMPLE_RATE * AUDIO_DUAL_RELEASE_MS) / 1000U))
#ifndef AUDIO_GG_INPUT_FULL_SCALE_VPEAK
#define AUDIO_GG_INPUT_FULL_SCALE_VPEAK \
    (AUDIO_ADC_FULL_SCALE_VPEAK / AUDIO_DUAL_HOT_GAIN)
#endif
#define AUDIO_GG_INPUT_FULL_SCALE_VRMS \
    (AUDIO_GG_INPUT_FULL_SCALE_VPEAK * 0.7071067811865475f)

/* Phase-1 analog front end. These are nominal resistor ratios, not a factory
 * calibration. The correction factors are the one-point calibration hook:
 * known input Vrms / nominal displayed input Vrms. */
#define AUDIO_FRONTEND_RF_OHMS      15000.0f
#define AUDIO_FRONTEND_LINE_RG_OHMS 15000.0f
#define AUDIO_FRONTEND_INST_RG_OHMS  2200.0f
#define AUDIO_FRONTEND_LINE_GAIN \
    (1.0f + AUDIO_FRONTEND_RF_OHMS / AUDIO_FRONTEND_LINE_RG_OHMS)
#define AUDIO_FRONTEND_INST_GAIN \
    (1.0f + AUDIO_FRONTEND_RF_OHMS / AUDIO_FRONTEND_INST_RG_OHMS)
#define AUDIO_FRONTEND_LINE_VOLTAGE_CORRECTION 1.0f
#define AUDIO_FRONTEND_INST_VOLTAGE_CORRECTION 1.0f

