#pragma once

#include <stdint.h>

#include "midi.h"

#define MIDI_HISTORY_CAP 16

typedef enum {
    MIDI_CAPTURE_NONE = 0,
    MIDI_CAPTURE_MONITOR,
} midi_capture_mode_t;

typedef struct {
    midi_msg_t message;
    uint32_t timestamp_ms;
    uint32_t sequence;
} midi_event_t;

typedef struct {
    midi_event_t history[MIDI_HISTORY_CAP];
    int history_count;
    uint32_t total_messages;
    uint32_t clock_messages;
    uint32_t program_sequence;
    midi_msg_t last_program;
    uint32_t last_program_ms;
} midi_service_snapshot_t;

void midi_service_init(void);
void midi_service_publish(const midi_msg_t *message, uint32_t timestamp_ms);
void midi_service_snapshot_get(midi_service_snapshot_t *out);
void midi_service_set_capture(midi_capture_mode_t mode);
midi_capture_mode_t midi_service_capture(void);
const char *midi_type_name(midi_type_t type);
