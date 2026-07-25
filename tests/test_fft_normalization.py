"""Reference checks for the spectrum normalization used by fft_map.c."""

import math


FFT_SIZE = 2048
BIN = 100
DISPLAY_SLOPE_DB_OCT = 4.5
DISPLAY_PIVOT_HZ = 1000.0


def measured_dbfs(amplitude: float) -> float:
    window = [
        0.5 - 0.5 * math.cos(2.0 * math.pi * n / (FFT_SIZE - 1))
        for n in range(FFT_SIZE)
    ]
    real = 0.0
    imag = 0.0
    for n, weight in enumerate(window):
        sample = amplitude * math.sin(2.0 * math.pi * BIN * n / FFT_SIZE)
        phase = -2.0 * math.pi * BIN * n / FFT_SIZE
        real += sample * weight * math.cos(phase)
        imag += sample * weight * math.sin(phase)
    raw_power = real * real + imag * imag
    normalized_power = 4.0 * raw_power / (sum(window) ** 2)
    return 10.0 * math.log10(normalized_power)


for amplitude in (1.0, 0.5, 0.1, 0.01):
    expected = 20.0 * math.log10(amplitude)
    actual = measured_dbfs(amplitude)
    assert abs(actual - expected) < 0.01, (amplitude, actual, expected)


def display_tilt(frequency: float) -> float:
    return DISPLAY_SLOPE_DB_OCT * math.log2(frequency / DISPLAY_PIVOT_HZ)


assert abs(display_tilt(DISPLAY_PIVOT_HZ)) < 1e-9
assert abs(display_tilt(2000.0) - DISPLAY_SLOPE_DB_OCT) < 1e-9
assert abs(display_tilt(500.0) + DISPLAY_SLOPE_DB_OCT) < 1e-9
assert display_tilt(20.0) < 0.0
assert display_tilt(20000.0) > 0.0

