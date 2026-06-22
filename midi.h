#pragma once
#include <stdint.h>
/* 파싱된 MIDI 메시지 */
typedef enum {
    MIDI_NONE, MIDI_NOTE_ON, MIDI_NOTE_OFF, MIDI_CC, MIDI_PC,
    MIDI_CLOCK, MIDI_START, MIDI_CONTINUE, MIDI_STOP, MIDI_SPP
} midi_type_t;
typedef struct { midi_type_t type; uint8_t ch, d1, d2; uint16_t pos14; } midi_msg_t;

void midi_feed(uint8_t b);                 /* UART RX 바이트마다 호출 */
void midi_reset(void);
void midi_on_message(const midi_msg_t* m); /* midi_map.c가 구현(콜백) */
