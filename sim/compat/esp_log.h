#pragma once

#include <stdio.h>

#define ESP_LOGI(tag, ...) do { \
    printf("I (%s) ", (tag) ? (tag) : "log"); \
    printf(__VA_ARGS__); \
    printf("\n"); \
} while (0)

#define ESP_LOGW(tag, ...) do { \
    printf("W (%s) ", (tag) ? (tag) : "log"); \
    printf(__VA_ARGS__); \
    printf("\n"); \
} while (0)

#define ESP_LOGE(tag, ...) do { \
    fprintf(stderr, "E (%s) ", (tag) ? (tag) : "log"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
} while (0)
