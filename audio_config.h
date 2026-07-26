#pragma once

/* Shared audio/DSP dimensions. Keep these in one place so producers and
 * renderers cannot silently disagree about buffer sizes. */
#define AUDIO_SAMPLE_RATE 48000
#define VIZ_POINTS        256

/* Shared spectrum display contract. Values are dBFS after display tilt. */
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

