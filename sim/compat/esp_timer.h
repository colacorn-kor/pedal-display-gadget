#pragma once

#include <stdint.h>

#include <SDL.h>

static inline int64_t esp_timer_get_time(void)
{
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    const uint64_t counter = SDL_GetPerformanceCounter();
    return frequency > 0
        ? (int64_t)((counter / frequency) * 1000000ULL +
                    (counter % frequency) * 1000000ULL / frequency)
        : (int64_t)SDL_GetTicks() * 1000;
}
