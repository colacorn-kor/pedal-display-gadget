#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "midi.h"

static midi_msg_t messages[16];
static size_t message_count;

void midi_on_message(const midi_msg_t *message)
{
    assert(message_count < sizeof(messages) / sizeof(messages[0]));
    messages[message_count++] = *message;
}

static void feed(const uint8_t *bytes, size_t count)
{
    for (size_t i = 0; i < count; i++) midi_feed(bytes[i]);
}

int main(void)
{
    midi_reset();
    const uint8_t stream[] = {
        0x90, 60, 100,       /* note on */
        61, 0,               /* running status, vel=0 -> note off */
        0xB0, 7, 0xF8, 64,  /* realtime byte interleaved with CC */
        0xF2, 1, 0xF8, 2,   /* realtime byte interleaved with SPP */
        3, 4,                /* must not become a running-status SPP */
        0xC0, 5, 6,         /* PC running status */
    };
    feed(stream, sizeof(stream));

    assert(message_count == 8);
    assert(messages[0].type == MIDI_NOTE_ON && messages[0].d1 == 60 && messages[0].d2 == 100);
    assert(messages[1].type == MIDI_NOTE_OFF && messages[1].d1 == 61 && messages[1].d2 == 0);
    assert(messages[2].type == MIDI_CLOCK);
    assert(messages[3].type == MIDI_CC && messages[3].d1 == 7 && messages[3].d2 == 64);
    assert(messages[4].type == MIDI_CLOCK);
    assert(messages[5].type == MIDI_SPP && messages[5].pos14 == 257);
    assert(messages[6].type == MIDI_PC && messages[6].d1 == 5);
    assert(messages[7].type == MIDI_PC && messages[7].d1 == 6);
    return 0;
}

