#include <stdio.h>
#include <string.h>

#include "midi_service.h"

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    midi_service_init();
    if (midi_service_capture() != MIDI_CAPTURE_NONE) {
        return fail("initial capture mode");
    }

    midi_msg_t note = {
        .type = MIDI_NOTE_ON, .ch = 1, .d1 = 60, .d2 = 100,
    };
    midi_msg_t clock = { .type = MIDI_CLOCK };
    midi_msg_t program = { .type = MIDI_PC, .ch = 4, .d1 = 17 };
    midi_service_publish(&note, 10);
    midi_service_publish(&clock, 11);
    midi_service_publish(&program, 12);

    midi_service_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    midi_service_snapshot_get(&snapshot);
    if (snapshot.total_messages != 3 || snapshot.clock_messages != 1 ||
        snapshot.history_count != 2 ||
        snapshot.history[0].message.type != MIDI_PC ||
        snapshot.history[0].sequence != 3 ||
        snapshot.history[1].message.type != MIDI_NOTE_ON ||
        snapshot.history[1].sequence != 1) {
        return fail("history and clock accounting");
    }
    if (snapshot.program_sequence != 1 ||
        snapshot.last_program.type != MIDI_PC ||
        snapshot.last_program.ch != 4 || snapshot.last_program.d1 != 17 ||
        snapshot.last_program_ms != 12) {
        return fail("latest Program Change");
    }

    for (int i = 0; i < MIDI_HISTORY_CAP + 4; i++) {
        note.d1 = (uint8_t)i;
        midi_service_publish(&note, (uint32_t)(20 + i));
    }
    midi_service_snapshot_get(&snapshot);
    if (snapshot.history_count != MIDI_HISTORY_CAP ||
        snapshot.history[0].message.d1 != MIDI_HISTORY_CAP + 3) {
        return fail("bounded history");
    }

    midi_service_set_capture(MIDI_CAPTURE_MONITOR);
    if (midi_service_capture() != MIDI_CAPTURE_MONITOR) {
        return fail("Monitor capture mode");
    }
    midi_service_set_capture((midi_capture_mode_t)99);
    if (midi_service_capture() != MIDI_CAPTURE_NONE) {
        return fail("invalid capture fallback");
    }
    if (strcmp(midi_type_name(MIDI_PC), "PROGRAM") != 0 ||
        strcmp(midi_type_name(MIDI_SPP), "POSITION") != 0) {
        return fail("type names");
    }

    printf("PASS: MIDI service history, Program Change, and capture mode\n");
    return 0;
}
