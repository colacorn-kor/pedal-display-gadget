#include "sim_midi.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midi.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#define MIDI_QUEUE_CAP 256u

static uint32_t s_queue[MIDI_QUEUE_CAP];
static atomic_uint s_queue_write;
static atomic_uint s_queue_read;
static atomic_uint s_dropped;
static atomic_uint s_received;
static atomic_uint s_sent;
static bool s_initialized;
static bool s_virtual_ports;
static bool s_exit_after_args;
static bool s_input_available;
static bool s_output_available;
static char s_input_name[64];
static char s_output_name[64];

#ifdef _WIN32
static HMIDIIN s_input;
static HMIDIOUT s_output;
#endif

static bool parse_index(const char *text, int *out)
{
    char *end = NULL;
    if (!text || !*text || !out) return false;
    const long value = strtol(text, &end, 10);
    if (!end || *end || value < 0 || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool enqueue_short(uint32_t packed)
{
    const unsigned write = atomic_load_explicit(
        &s_queue_write, memory_order_relaxed);
    const unsigned next = (write + 1u) % MIDI_QUEUE_CAP;
    if (next == atomic_load_explicit(&s_queue_read, memory_order_acquire)) {
        atomic_fetch_add_explicit(&s_dropped, 1u, memory_order_relaxed);
        return false;
    }
    s_queue[write] = packed;
    atomic_store_explicit(&s_queue_write, next, memory_order_release);
    return true;
}

#ifdef _WIN32
static void CALLBACK midi_input_callback(HMIDIIN input, UINT message,
                                         DWORD_PTR instance,
                                         DWORD_PTR param1, DWORD_PTR param2)
{
    (void)input;
    (void)instance;
    (void)param2;
    if (message == MIM_DATA || message == MIM_MOREDATA) {
        (void)enqueue_short((uint32_t)param1);
    }
}

static void wide_name_to_utf8(const wchar_t *wide, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!wide) return;
    const int converted = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
    if (converted <= 0) (void)snprintf(out, out_size, "MIDI device");
}

static void input_device_name(UINT index, char *out, size_t out_size)
{
    MIDIINCAPSW caps;
    if (midiInGetDevCapsW(index, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        wide_name_to_utf8(caps.szPname, out, out_size);
    } else {
        (void)snprintf(out, out_size, "MIDI input %u", index);
    }
}

static void output_device_name(UINT index, char *out, size_t out_size)
{
    MIDIOUTCAPSW caps;
    if (midiOutGetDevCapsW(index, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        wide_name_to_utf8(caps.szPname, out, out_size);
    } else {
        (void)snprintf(out, out_size, "MIDI output %u", index);
    }
}

static void list_devices(void)
{
    const UINT input_count = midiInGetNumDevs();
    const UINT output_count = midiOutGetNumDevs();
    printf("MIDI input devices:\n");
    if (input_count == 0) printf("  (none)\n");
    for (UINT i = 0; i < input_count; i++) {
        char name[64];
        input_device_name(i, name, sizeof(name));
        printf("  %u: %s\n", i, name);
    }
    printf("MIDI output devices:\n");
    if (output_count == 0) printf("  (none)\n");
    for (UINT i = 0; i < output_count; i++) {
        char name[64];
        output_device_name(i, name, sizeof(name));
        printf("  %u: %s\n", i, name);
    }
}

static bool open_input(int index, bool explicit_index)
{
    const UINT count = midiInGetNumDevs();
    if (count == 0) return !explicit_index;
    if (index < 0) index = 0;
    if ((UINT)index >= count) {
        fprintf(stderr, "E (sim) MIDI input index out of range: %d\n", index);
        return false;
    }
    const MMRESULT result = midiInOpen(
        &s_input, (UINT)index, (DWORD_PTR)midi_input_callback, 0,
        CALLBACK_FUNCTION | MIDI_IO_STATUS);
    if (result != MMSYSERR_NOERROR || !s_input) {
        fprintf(stderr, "W (sim) MIDI input %d could not be opened\n", index);
        return !explicit_index;
    }
    if (midiInStart(s_input) != MMSYSERR_NOERROR) {
        midiInClose(s_input);
        s_input = NULL;
        fprintf(stderr, "W (sim) MIDI input %d could not be started\n", index);
        return !explicit_index;
    }
    input_device_name((UINT)index, s_input_name, sizeof(s_input_name));
    s_input_available = true;
    printf("I (sim) MIDI input: %s\n", s_input_name);
    return true;
}

static bool open_output(int index, bool explicit_index)
{
    const UINT count = midiOutGetNumDevs();
    if (count == 0) return !explicit_index;
    if (index < 0) index = 0;
    if ((UINT)index >= count) {
        fprintf(stderr, "E (sim) MIDI output index out of range: %d\n", index);
        return false;
    }
    const MMRESULT result = midiOutOpen(
        &s_output, (UINT)index, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR || !s_output) {
        fprintf(stderr, "W (sim) MIDI output %d could not be opened\n", index);
        return !explicit_index;
    }
    output_device_name((UINT)index, s_output_name, sizeof(s_output_name));
    s_output_available = true;
    printf("I (sim) MIDI output: %s\n", s_output_name);
    return true;
}
#else
static void list_devices(void)
{
    printf("MIDI input devices:\n  (not available on this platform)\n");
    printf("MIDI output devices:\n  (not available on this platform)\n");
}
#endif

bool sim_midi_init(int argc, char **argv, bool virtual_ports)
{
    int input_index = -1;
    int output_index = -1;
    bool input_explicit = false;
    bool output_explicit = false;
    bool disabled = false;

    if (s_initialized) return true;
    s_initialized = true;
    atomic_init(&s_queue_write, 0u);
    atomic_init(&s_queue_read, 0u);
    atomic_init(&s_dropped, 0u);
    atomic_init(&s_received, 0u);
    atomic_init(&s_sent, 0u);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list-midi") == 0) {
            s_exit_after_args = true;
        } else if (strcmp(argv[i], "--no-midi") == 0) {
            disabled = true;
        } else if (strcmp(argv[i], "--midi-in") == 0) {
            if (i + 1 >= argc || !parse_index(argv[++i], &input_index)) {
                fprintf(stderr, "E (sim) invalid --midi-in index\n");
                return false;
            }
            input_explicit = true;
        } else if (strcmp(argv[i], "--midi-out") == 0) {
            if (i + 1 >= argc || !parse_index(argv[++i], &output_index)) {
                fprintf(stderr, "E (sim) invalid --midi-out index\n");
                return false;
            }
            output_explicit = true;
        }
    }

    if (s_exit_after_args) {
        list_devices();
        return true;
    }
    if (disabled) return true;
    if (virtual_ports) {
        s_virtual_ports = true;
        s_input_available = true;
        s_output_available = true;
        (void)snprintf(s_input_name, sizeof(s_input_name), "Virtual MIDI In");
        (void)snprintf(s_output_name, sizeof(s_output_name), "Virtual MIDI Out");
        printf("I (sim) virtual MIDI input/output for automated checks\n");
        return true;
    }

#ifdef _WIN32
    if (!open_input(input_index, input_explicit) ||
        !open_output(output_index, output_explicit)) {
        sim_midi_shutdown();
        return false;
    }
#else
    (void)input_index;
    (void)output_index;
    (void)input_explicit;
    (void)output_explicit;
#endif
    (void)atexit(sim_midi_shutdown);
    return true;
}

void sim_midi_shutdown(void)
{
#ifdef _WIN32
    if (s_input) {
        midiInStop(s_input);
        midiInReset(s_input);
        midiInClose(s_input);
    }
    if (s_output) {
        midiOutReset(s_output);
        midiOutClose(s_output);
    }
    s_input = NULL;
    s_output = NULL;
#endif
    s_input_available = false;
    s_output_available = false;
}

static int short_message_length(uint8_t status)
{
    if (status >= 0xf8u) return 1;
    if (status == 0xf2u) return 3;
    if (status >= 0xf0u) return 1;
    const uint8_t high = status & 0xf0u;
    return high == 0xc0u || high == 0xd0u ? 2 : 3;
}

void sim_midi_pump(void)
{
    for (;;) {
        const unsigned read = atomic_load_explicit(
            &s_queue_read, memory_order_relaxed);
        if (read == atomic_load_explicit(
                        &s_queue_write, memory_order_acquire)) {
            return;
        }
        const uint32_t packed = s_queue[read];
        atomic_store_explicit(
            &s_queue_read, (read + 1u) % MIDI_QUEUE_CAP,
            memory_order_release);

        const uint8_t status = (uint8_t)(packed & 0xffu);
        const int length = short_message_length(status);
        midi_feed(status);
        if (length >= 2) midi_feed((uint8_t)((packed >> 8) & 0x7fu));
        if (length >= 3) midi_feed((uint8_t)((packed >> 16) & 0x7fu));
        atomic_fetch_add_explicit(&s_received, 1u, memory_order_relaxed);
    }
}

bool sim_midi_should_exit_after_args(void)
{
    return s_exit_after_args;
}

void sim_midi_get_status(platform_midi_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->input_available = s_input_available;
    out->output_available = s_output_available;
    (void)snprintf(out->input_name, sizeof(out->input_name), "%s",
                   s_input_name);
    (void)snprintf(out->output_name, sizeof(out->output_name), "%s",
                   s_output_name);
    out->received_messages = atomic_load_explicit(
        &s_received, memory_order_relaxed);
    out->dropped_messages = atomic_load_explicit(
        &s_dropped, memory_order_relaxed);
    out->sent_messages = atomic_load_explicit(
        &s_sent, memory_order_relaxed);
}

bool sim_midi_send_short(uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!s_output_available || !(status & 0x80u)) return false;
    const uint32_t packed = (uint32_t)status |
        ((uint32_t)(data1 & 0x7fu) << 8) |
        ((uint32_t)(data2 & 0x7fu) << 16);
    if (s_virtual_ports) {
        atomic_fetch_add_explicit(&s_sent, 1u, memory_order_relaxed);
        return true;
    }
#ifdef _WIN32
    if (s_output && midiOutShortMsg(s_output, (DWORD)packed) ==
                        MMSYSERR_NOERROR) {
        atomic_fetch_add_explicit(&s_sent, 1u, memory_order_relaxed);
        return true;
    }
#else
    (void)packed;
#endif
    return false;
}

bool sim_midi_inject_short(uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!s_virtual_ports || !s_input_available || !(status & 0x80u)) {
        return false;
    }
    return enqueue_short((uint32_t)status |
                         ((uint32_t)(data1 & 0x7fu) << 8) |
                         ((uint32_t)(data2 & 0x7fu) << 16));
}
