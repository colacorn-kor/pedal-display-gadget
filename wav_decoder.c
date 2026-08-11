#include "wav_decoder.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "audio_playback.h"

#define WAV_FRAME_BYTES_MAX 64u
#define WAV_FMT_BYTES_MAX 64u

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static bool read_exact(FILE *file, void *buffer, size_t bytes)
{
    return bytes == 0u || fread(buffer, 1u, bytes, file) == bytes;
}

static bool skip_bytes(FILE *file, uint32_t bytes)
{
    if (bytes > (uint32_t)LONG_MAX) return false;
    return fseek(file, (long)bytes, SEEK_CUR) == 0;
}

static bool chunk_id_is(const uint8_t id[4], const char *expected)
{
    return memcmp(id, expected, 4u) == 0;
}

static float decode_sample(const uint8_t *bytes,
                           uint16_t format,
                           uint16_t bits)
{
    if (format == 3u && bits == 32u) {
        const uint32_t raw = read_le32(bytes);
        float sample;
        memcpy(&sample, &raw, sizeof(sample));
        if (!isfinite(sample)) return 0.0f;
        if (sample > 1.0f) return 1.0f;
        if (sample < -1.0f) return -1.0f;
        return sample;
    }

    if (bits == 8u) {
        return ((float)bytes[0] - 128.0f) / 128.0f;
    }
    if (bits == 16u) {
        const int16_t value = (int16_t)read_le16(bytes);
        return (float)value / 32768.0f;
    }
    if (bits == 24u) {
        int32_t value = (int32_t)bytes[0] |
                        ((int32_t)bytes[1] << 8) |
                        ((int32_t)bytes[2] << 16);
        if (value & 0x00800000) value |= (int32_t)0xff000000;
        return (float)value / 8388608.0f;
    }

    const int32_t value = (int32_t)read_le32(bytes);
    return (float)((double)value / 2147483648.0);
}

static bool read_source_frame(wav_decoder_t *decoder, float out[2])
{
    uint8_t frame[WAV_FRAME_BYTES_MAX];
    if (decoder->data_remaining < decoder->block_align) return false;
    if (!read_exact(decoder->file, frame, decoder->block_align)) {
        decoder->error = WAV_DECODER_ERR_TRUNCATED;
        decoder->data_remaining = 0u;
        return false;
    }
    decoder->data_remaining -= decoder->block_align;

    const uint16_t bytes_per_sample = decoder->bits_per_sample / 8u;
    out[0] = decode_sample(
        frame, decoder->format, decoder->bits_per_sample);
    out[1] = decoder->channels == 1u
        ? out[0]
        : decode_sample(frame + bytes_per_sample,
                        decoder->format, decoder->bits_per_sample);
    return true;
}

static bool parse_format(wav_decoder_t *decoder,
                         const uint8_t *format,
                         uint32_t size)
{
    if (size < 16u) return false;

    uint16_t encoding = read_le16(format);
    const uint16_t channels = read_le16(format + 2u);
    const uint32_t sample_rate = read_le32(format + 4u);
    const uint16_t block_align = read_le16(format + 12u);
    const uint16_t bits = read_le16(format + 14u);

    if (encoding == 0xfffeu && size >= 40u) {
        encoding = read_le16(format + 24u);
    }
    if ((encoding != 1u && encoding != 3u) ||
        channels < 1u || channels > 2u ||
        sample_rate < 8000u || sample_rate > 192000u ||
        block_align == 0u || block_align > WAV_FRAME_BYTES_MAX) {
        return false;
    }
    if (encoding == 1u &&
        bits != 8u && bits != 16u && bits != 24u && bits != 32u) {
        return false;
    }
    if (encoding == 3u && bits != 32u) return false;

    const uint16_t bytes_per_sample = bits / 8u;
    if (block_align < channels * bytes_per_sample) return false;

    decoder->format = encoding;
    decoder->channels = channels;
    decoder->sample_rate = sample_rate;
    decoder->block_align = block_align;
    decoder->bits_per_sample = bits;
    return true;
}

bool wav_decoder_open(wav_decoder_t *decoder, FILE *file)
{
    uint8_t header[12];
    uint8_t chunk_header[8];
    uint8_t format[WAV_FMT_BYTES_MAX];
    bool have_format = false;
    bool have_data = false;
    long data_offset = -1;
    uint32_t data_size = 0u;

    if (!decoder) {
        if (file) fclose(file);
        return false;
    }
    memset(decoder, 0, sizeof(*decoder));
    decoder->file = file;
    decoder->error = WAV_DECODER_ERR_IO;
    if (!file || !read_exact(file, header, sizeof(header))) return false;
    if (!chunk_id_is(header, "RIFF") ||
        !chunk_id_is(header + 8u, "WAVE")) {
        decoder->error = WAV_DECODER_ERR_CONTAINER;
        return false;
    }

    while (read_exact(file, chunk_header, sizeof(chunk_header))) {
        const uint32_t chunk_size = read_le32(chunk_header + 4u);
        const long payload_offset = ftell(file);
        if (payload_offset < 0) break;

        if (chunk_id_is(chunk_header, "fmt ")) {
            const uint32_t copied = chunk_size < WAV_FMT_BYTES_MAX
                ? chunk_size : WAV_FMT_BYTES_MAX;
            if (!read_exact(file, format, copied) ||
                !parse_format(decoder, format, copied) ||
                !skip_bytes(file, chunk_size - copied)) {
                decoder->error = WAV_DECODER_ERR_FORMAT;
                return false;
            }
            have_format = true;
        } else if (chunk_id_is(chunk_header, "data")) {
            data_offset = payload_offset;
            data_size = chunk_size;
            have_data = true;
            if (have_format) break;
            if (!skip_bytes(file, chunk_size)) break;
        } else if (!skip_bytes(file, chunk_size)) {
            break;
        }

        if ((chunk_size & 1u) && fseek(file, 1L, SEEK_CUR) != 0) break;
        if (have_format && have_data) break;
    }

    if (!have_format || !have_data || data_offset < 0) {
        decoder->error = WAV_DECODER_ERR_CONTAINER;
        return false;
    }
    if (fseek(file, data_offset, SEEK_SET) != 0) {
        decoder->error = WAV_DECODER_ERR_IO;
        return false;
    }

    decoder->data_remaining = data_size;
    decoder->total_source_frames = data_size / decoder->block_align;
    if (decoder->total_source_frames == 0u) {
        decoder->error = WAV_DECODER_ERR_EMPTY;
        return false;
    }
    decoder->total_output_frames =
        ((uint64_t)decoder->total_source_frames *
         AUDIO_PLAYBACK_SAMPLE_RATE + decoder->sample_rate - 1u) /
        decoder->sample_rate;
    decoder->source_step =
        (double)decoder->sample_rate / AUDIO_PLAYBACK_SAMPLE_RATE;
    decoder->error = WAV_DECODER_OK;

    if (!read_source_frame(decoder, decoder->frame_a)) {
        decoder->error = WAV_DECODER_ERR_TRUNCATED;
        return false;
    }
    if (!read_source_frame(decoder, decoder->frame_b)) {
        decoder->frame_b[0] = decoder->frame_a[0];
        decoder->frame_b[1] = decoder->frame_a[1];
        decoder->last_frame = true;
        if (decoder->error == WAV_DECODER_ERR_TRUNCATED) return false;
    }
    return true;
}

void wav_decoder_close(wav_decoder_t *decoder)
{
    if (!decoder) return;
    if (decoder->file) fclose(decoder->file);
    memset(decoder, 0, sizeof(*decoder));
}

size_t wav_decoder_read(wav_decoder_t *decoder,
                        float *stereo_interleaved,
                        size_t output_frames)
{
    if (!decoder || !decoder->file || !stereo_interleaved ||
        decoder->error != WAV_DECODER_OK || decoder->finished) {
        return 0u;
    }

    size_t produced = 0u;
    while (produced < output_frames && !decoder->finished) {
        const float fraction = (float)decoder->phase;
        stereo_interleaved[produced * 2u] =
            decoder->frame_a[0] +
            (decoder->frame_b[0] - decoder->frame_a[0]) * fraction;
        stereo_interleaved[produced * 2u + 1u] =
            decoder->frame_a[1] +
            (decoder->frame_b[1] - decoder->frame_a[1]) * fraction;
        produced++;
        decoder->output_frames++;
        decoder->phase += decoder->source_step;

        while (decoder->phase >= 1.0 && !decoder->finished) {
            decoder->phase -= 1.0;
            if (decoder->last_frame) {
                decoder->finished = true;
                break;
            }
            decoder->frame_a[0] = decoder->frame_b[0];
            decoder->frame_a[1] = decoder->frame_b[1];
            if (!read_source_frame(decoder, decoder->frame_b)) {
                if (decoder->error == WAV_DECODER_OK) {
                    decoder->frame_b[0] = decoder->frame_a[0];
                    decoder->frame_b[1] = decoder->frame_a[1];
                    decoder->last_frame = true;
                } else {
                    decoder->finished = true;
                }
            }
        }
        if (decoder->output_frames >= decoder->total_output_frames) {
            decoder->finished = true;
        }
    }
    return produced;
}

bool wav_decoder_finished(const wav_decoder_t *decoder)
{
    return !decoder || decoder->finished;
}

uint32_t wav_decoder_position_ms(const wav_decoder_t *decoder)
{
    if (!decoder) return 0u;
    return (uint32_t)(decoder->output_frames * 1000u /
                      AUDIO_PLAYBACK_SAMPLE_RATE);
}

uint32_t wav_decoder_duration_ms(const wav_decoder_t *decoder)
{
    if (!decoder) return 0u;
    return (uint32_t)(decoder->total_output_frames * 1000u /
                      AUDIO_PLAYBACK_SAMPLE_RATE);
}

const char *wav_decoder_error_text(wav_decoder_error_t error)
{
    switch (error) {
    case WAV_DECODER_OK: return "Ready";
    case WAV_DECODER_ERR_IO: return "File read error";
    case WAV_DECODER_ERR_CONTAINER: return "Invalid WAV container";
    case WAV_DECODER_ERR_FORMAT: return "Unsupported WAV format";
    case WAV_DECODER_ERR_EMPTY: return "Empty WAV file";
    case WAV_DECODER_ERR_TRUNCATED: return "Truncated WAV file";
    default: return "WAV error";
    }
}
