#include "sim_loopback_win.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propvarutil.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "audio_config.h"

enum sample_format_t {
    SAMPLE_FORMAT_NONE = 0,
    SAMPLE_FORMAT_FLOAT32,
    SAMPLE_FORMAT_PCM16,
    SAMPLE_FORMAT_PCM24,
    SAMPLE_FORMAT_PCM32,
};

static IMMDeviceEnumerator *s_enumerator;
static IMMDevice *s_device;
static IAudioClient *s_audio_client;
static IAudioCaptureClient *s_capture_client;
static WAVEFORMATEX *s_mix_format;
static sample_format_t s_sample_format;
static bool s_com_owned;
static bool s_started;
static bool s_have_previous;
static float s_previous_sample;
static uint64_t s_source_frame;
static double s_next_output_position;
static double s_source_frames_per_output;

static void report_error(const char *operation, HRESULT result)
{
    std::fprintf(stderr, "E (sim) WASAPI %s failed: 0x%08lx\n",
                 operation, static_cast<unsigned long>(result));
}

static void release_interfaces(void)
{
    if (s_audio_client && s_started) {
        s_audio_client->Stop();
    }
    s_started = false;

    if (s_capture_client) {
        s_capture_client->Release();
        s_capture_client = nullptr;
    }
    if (s_audio_client) {
        s_audio_client->Release();
        s_audio_client = nullptr;
    }
    if (s_device) {
        s_device->Release();
        s_device = nullptr;
    }
    if (s_enumerator) {
        s_enumerator->Release();
        s_enumerator = nullptr;
    }
    if (s_mix_format) {
        CoTaskMemFree(s_mix_format);
        s_mix_format = nullptr;
    }
    if (s_com_owned) {
        CoUninitialize();
        s_com_owned = false;
    }

    s_sample_format = SAMPLE_FORMAT_NONE;
    s_have_previous = false;
    s_previous_sample = 0.0f;
    s_source_frame = 0;
    s_next_output_position = 0.0;
    s_source_frames_per_output = 1.0;
}

static bool classify_format(const WAVEFORMATEX *format)
{
    if (!format || format->nChannels == 0 ||
        format->nSamplesPerSec == 0 || format->nBlockAlign == 0) {
        return false;
    }

    WORD tag = format->wFormatTag;
    GUID subtype = GUID_NULL;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        if (format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) -
                                 sizeof(WAVEFORMATEX)) {
            return false;
        }
        const auto *extensible =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format);
        subtype = extensible->SubFormat;
        if (IsEqualGUID(subtype, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            tag = WAVE_FORMAT_IEEE_FLOAT;
        } else if (IsEqualGUID(subtype, KSDATAFORMAT_SUBTYPE_PCM)) {
            tag = WAVE_FORMAT_PCM;
        } else {
            return false;
        }
    }

    if (tag == WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample == 32) {
        s_sample_format = SAMPLE_FORMAT_FLOAT32;
        return true;
    }
    if (tag != WAVE_FORMAT_PCM) return false;

    if (format->wBitsPerSample == 16) {
        s_sample_format = SAMPLE_FORMAT_PCM16;
        return true;
    }
    if (format->wBitsPerSample == 24) {
        s_sample_format = SAMPLE_FORMAT_PCM24;
        return true;
    }
    if (format->wBitsPerSample == 32) {
        s_sample_format = SAMPLE_FORMAT_PCM32;
        return true;
    }
    return false;
}

static float decode_channel(const BYTE *sample)
{
    if (!sample) return 0.0f;

    if (s_sample_format == SAMPLE_FORMAT_FLOAT32) {
        float value;
        std::memcpy(&value, sample, sizeof(value));
        return std::isfinite(value) ? value : 0.0f;
    }
    if (s_sample_format == SAMPLE_FORMAT_PCM16) {
        int16_t value;
        std::memcpy(&value, sample, sizeof(value));
        return static_cast<float>(value) / 32768.0f;
    }
    if (s_sample_format == SAMPLE_FORMAT_PCM24) {
        int32_t value =
            static_cast<int32_t>(sample[0]) |
            (static_cast<int32_t>(sample[1]) << 8) |
            (static_cast<int32_t>(sample[2]) << 16);
        if (value & 0x00800000) value |= ~0x00ffffff;
        return static_cast<float>(value) / 8388608.0f;
    }
    if (s_sample_format == SAMPLE_FORMAT_PCM32) {
        int32_t value;
        std::memcpy(&value, sample, sizeof(value));
        return static_cast<float>(
            static_cast<double>(value) / 2147483648.0);
    }
    return 0.0f;
}

static float decode_mono_frame(const BYTE *frame)
{
    if (!frame || !s_mix_format) return 0.0f;

    const int channels = static_cast<int>(s_mix_format->nChannels);
    const int bytes_per_channel =
        static_cast<int>(s_mix_format->nBlockAlign) / channels;
    float sample = decode_channel(frame);
    if (channels >= 2) {
        sample = 0.5f *
                 (sample + decode_channel(frame + bytes_per_channel));
    }
    if (!std::isfinite(sample)) return 0.0f;
    if (sample < -1.0f) return -1.0f;
    if (sample > 1.0f) return 1.0f;
    return sample;
}

static void resample(float sample, sim_loopback_sample_fn on_sample,
                     void *context)
{
    if (!s_have_previous) {
        s_have_previous = true;
        s_previous_sample = sample;
        s_source_frame = 0;
        s_next_output_position = s_source_frames_per_output;
        on_sample(sample, context);
        return;
    }

    s_source_frame++;
    const double segment_start =
        static_cast<double>(s_source_frame - 1U);
    const double segment_end = static_cast<double>(s_source_frame);
    while (s_next_output_position <= segment_end + 1e-9) {
        double mix = s_next_output_position - segment_start;
        if (mix < 0.0) mix = 0.0;
        if (mix > 1.0) mix = 1.0;
        const float output = s_previous_sample +
            static_cast<float>(mix) * (sample - s_previous_sample);
        on_sample(output, context);
        s_next_output_position += s_source_frames_per_output;
    }
    s_previous_sample = sample;
}

static void read_device_name(char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    std::snprintf(out, out_size, "Windows default output");

    IPropertyStore *properties = nullptr;
    PROPVARIANT value;
    PropVariantInit(&value);
    HRESULT result =
        s_device->OpenPropertyStore(STGM_READ, &properties);
    if (SUCCEEDED(result)) {
        result = properties->GetValue(PKEY_Device_FriendlyName, &value);
    }
    if (SUCCEEDED(result) && value.vt == VT_LPWSTR && value.pwszVal) {
        WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1,
                            out, static_cast<int>(out_size),
                            nullptr, nullptr);
        out[out_size - 1] = '\0';
    }
    PropVariantClear(&value);
    if (properties) properties->Release();
}

extern "C" bool sim_loopback_open(char *device_name,
                                    size_t device_name_size,
                                    int *sample_rate, int *channels)
{
    sim_loopback_close();

    HRESULT result =
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(result)) {
        s_com_owned = true;
    } else if (result != RPC_E_CHANGED_MODE) {
        report_error("CoInitializeEx", result);
        release_interfaces();
        return false;
    }

    result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void **>(&s_enumerator));
    if (FAILED(result)) {
        report_error("CoCreateInstance", result);
        release_interfaces();
        return false;
    }

    result = s_enumerator->GetDefaultAudioEndpoint(
        eRender, eMultimedia, &s_device);
    if (FAILED(result)) {
        report_error("GetDefaultAudioEndpoint", result);
        release_interfaces();
        return false;
    }

    result = s_device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void **>(&s_audio_client));
    if (FAILED(result)) {
        report_error("Activate", result);
        release_interfaces();
        return false;
    }

    result = s_audio_client->GetMixFormat(&s_mix_format);
    if (FAILED(result)) {
        report_error("GetMixFormat", result);
        release_interfaces();
        return false;
    }
    if (!classify_format(s_mix_format)) {
        std::fprintf(
            stderr,
            "E (sim) unsupported WASAPI mix format: tag %u, %u bit, %u ch\n",
            static_cast<unsigned>(s_mix_format->wFormatTag),
            static_cast<unsigned>(s_mix_format->wBitsPerSample),
            static_cast<unsigned>(s_mix_format->nChannels));
        release_interfaces();
        return false;
    }

    constexpr REFERENCE_TIME buffer_duration_100ns = 1000000;
    result = s_audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        buffer_duration_100ns, 0, s_mix_format, nullptr);
    if (FAILED(result)) {
        report_error("Initialize", result);
        release_interfaces();
        return false;
    }

    result = s_audio_client->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void **>(&s_capture_client));
    if (FAILED(result)) {
        report_error("GetService", result);
        release_interfaces();
        return false;
    }

    result = s_audio_client->Start();
    if (FAILED(result)) {
        report_error("Start", result);
        release_interfaces();
        return false;
    }
    s_started = true;
    s_source_frames_per_output =
        static_cast<double>(s_mix_format->nSamplesPerSec) /
        static_cast<double>(AUDIO_SAMPLE_RATE);

    read_device_name(device_name, device_name_size);
    if (sample_rate) {
        *sample_rate =
            static_cast<int>(s_mix_format->nSamplesPerSec);
    }
    if (channels) {
        *channels = static_cast<int>(s_mix_format->nChannels);
    }
    return true;
}

extern "C" bool sim_loopback_pump(
    sim_loopback_sample_fn on_sample, void *context)
{
    if (!s_capture_client || !s_mix_format || !on_sample) return false;

    UINT32 packet_frames = 0;
    HRESULT result =
        s_capture_client->GetNextPacketSize(&packet_frames);
    if (FAILED(result)) {
        report_error("GetNextPacketSize", result);
        return false;
    }

    while (packet_frames > 0) {
        BYTE *data = nullptr;
        DWORD flags = 0;
        UINT64 device_position = 0;
        UINT64 qpc_position = 0;
        result = s_capture_client->GetBuffer(
            &data, &packet_frames, &flags,
            &device_position, &qpc_position);
        if (FAILED(result)) {
            report_error("GetBuffer", result);
            return false;
        }

        if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
            s_have_previous = false;
        }
        const bool silent =
            (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || !data;
        for (UINT32 i = 0; i < packet_frames; i++) {
            float sample = 0.0f;
            if (!silent) {
                sample = decode_mono_frame(
                    data + static_cast<size_t>(i) *
                               s_mix_format->nBlockAlign);
            }
            resample(sample, on_sample, context);
        }

        result = s_capture_client->ReleaseBuffer(packet_frames);
        if (FAILED(result)) {
            report_error("ReleaseBuffer", result);
            return false;
        }

        result = s_capture_client->GetNextPacketSize(&packet_frames);
        if (FAILED(result)) {
            report_error("GetNextPacketSize", result);
            return false;
        }
    }
    return true;
}

extern "C" void sim_loopback_close(void)
{
    release_interfaces();
}
