"""Reference checks for the spectrum normalization used by fft_map.c."""

import math


FFT_SIZE = 2048
BIN = 100
SAMPLE_RATE = 48000.0
LOW_DECIMATION = 4
ULTRA_DECIMATION = 4
DC_BLOCK_HZ = 2.0


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


high_bin_hz = SAMPLE_RATE / FFT_SIZE
low_bin_hz = SAMPLE_RATE / LOW_DECIMATION / FFT_SIZE
ultra_bin_hz = (
    SAMPLE_RATE / LOW_DECIMATION / ULTRA_DECIMATION / FFT_SIZE
)
assert abs(high_bin_hz - 23.4375) < 1e-9
assert abs(low_bin_hz - 5.859375) < 1e-9
assert abs(ultra_bin_hz - 1.46484375) < 1e-9


def boxcar_gain(frequency: float) -> float:
    numerator = math.sin(math.pi * frequency * LOW_DECIMATION / SAMPLE_RATE)
    denominator = LOW_DECIMATION * math.sin(math.pi * frequency / SAMPLE_RATE)
    return abs(numerator / denominator)


# Two cascaded four-sample boxcars form the 7-tap triangular decimator. It is
# effectively flat in the low-resolution branch and strongly rejects aliases
# around its first 12kHz null.
assert 40.0 * math.log10(boxcar_gain(300.0)) > -0.02
assert 40.0 * math.log10(boxcar_gain(11900.0)) < -75.0


def dc_block_gain(frequency: float) -> float:
    coefficient = math.exp(-2.0 * math.pi * DC_BLOCK_HZ / SAMPLE_RATE)
    omega = 2.0 * math.pi * frequency / SAMPLE_RATE
    numerator = abs(1.0 - complex(math.cos(-omega), math.sin(-omega)))
    denominator = abs(
        1.0 - coefficient *
        complex(math.cos(-omega), math.sin(-omega))
    )
    return numerator / denominator


# DC is rejected while 20Hz, the first visible point, remains effectively flat.
assert dc_block_gain(0.0) == 0.0
assert 20.0 * math.log10(dc_block_gain(20.0)) > -0.05

