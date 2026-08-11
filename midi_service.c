#include "midi_service.h"

#include <stdatomic.h>
#include <string.h>

static atomic_uint s_publish_seq;
static atomic_int s_capture_mode;
static midi_service_snapshot_t s_state;

void midi_service_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    atomic_init(&s_publish_seq, 0u);
    atomic_init(&s_capture_mode, MIDI_CAPTURE_NONE);
}

void midi_service_publish(const midi_msg_t *message, uint32_t timestamp_ms)
{
    if (!message || message->type == MIDI_NONE) return;

    atomic_fetch_add_explicit(&s_publish_seq, 1u, memory_order_acq_rel);
    s_state.total_messages++;
    if (message->type == MIDI_CLOCK) {
        s_state.clock_messages++;
    } else {
        const int move = s_state.history_count < MIDI_HISTORY_CAP
            ? s_state.history_count : MIDI_HISTORY_CAP - 1;
        if (move > 0) {
            memmove(&s_state.history[1], &s_state.history[0],
                    (size_t)move * sizeof(s_state.history[0]));
        }
        s_state.history[0].message = *message;
        s_state.history[0].timestamp_ms = timestamp_ms;
        s_state.history[0].sequence = s_state.total_messages;
        if (s_state.history_count < MIDI_HISTORY_CAP) {
            s_state.history_count++;
        }
    }
    if (message->type == MIDI_PC) {
        s_state.last_program = *message;
        s_state.last_program_ms = timestamp_ms;
        s_state.program_sequence++;
    }
    atomic_fetch_add_explicit(&s_publish_seq, 1u, memory_order_release);
}

void midi_service_snapshot_get(midi_service_snapshot_t *out)
{
    if (!out) return;
    for (;;) {
        const unsigned before = atomic_load_explicit(
            &s_publish_seq, memory_order_acquire);
        if (before & 1u) continue;
        *out = s_state;
        atomic_thread_fence(memory_order_acquire);
        const unsigned after = atomic_load_explicit(
            &s_publish_seq, memory_order_relaxed);
        if (before == after) return;
    }
}

void midi_service_set_capture(midi_capture_mode_t mode)
{
    if (mode < MIDI_CAPTURE_NONE || mode > MIDI_CAPTURE_MONITOR) {
        mode = MIDI_CAPTURE_NONE;
    }
    atomic_store_explicit(&s_capture_mode, (int)mode, memory_order_release);
}

midi_capture_mode_t midi_service_capture(void)
{
    return (midi_capture_mode_t)atomic_load_explicit(
        &s_capture_mode, memory_order_acquire);
}

const char *midi_type_name(midi_type_t type)
{
    switch (type) {
    case MIDI_NOTE_ON: return "NOTE ON";
    case MIDI_NOTE_OFF: return "NOTE OFF";
    case MIDI_CC: return "CONTROL";
    case MIDI_PC: return "PROGRAM";
    case MIDI_CLOCK: return "CLOCK";
    case MIDI_START: return "START";
    case MIDI_CONTINUE: return "CONTINUE";
    case MIDI_STOP: return "STOP";
    case MIDI_SPP: return "POSITION";
    default: return "MIDI";
    }
}
