#pragma once

#include <stdint.h>

#include <SDL.h>

static inline int64_t esp_timer_get_time(void)
{
    return (int64_t)SDL_GetTicks() * 1000;
}
