#include "pc/audio/PcMusicStream.h"

#include "pc/external/stb_vorbis.h"
#include "platform/audio/VorbisAssetOpen.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

namespace
{
constexpr int kOutputSampleRate = 44100;
constexpr int kDecodeFrames = 4096;
constexpr std::size_t kRingSamples = static_cast<std::size_t>(kOutputSampleRate) * 2u;

std::array<float, kRingSamples> s_ring{};
std::size_t s_read = 0;
std::size_t s_write = 0;
std::size_t s_count = 0;
bool s_stop = false;
bool s_running = false;
bool s_eof = false;
std::thread s_thread;
std::mutex s_mutex;
std::condition_variable s_condition;

bool validateOgg(const std::string &path)
{
    int error = 0;
    stb_vorbis *vorbis = platformOpenVorbis(path, &error);
    if (vorbis == nullptr)
        return false;
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    stb_vorbis_close(vorbis);
    return info.channels >= 1 && info.channels <= 2 && info.sample_rate == kOutputSampleRate;
}

void decoderThread(std::string path)
{
    int error = 0;
    stb_vorbis *vorbis = platformOpenVorbis(path, &error);
    if (vorbis == nullptr)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_eof = true;
        s_running = false;
        return;
    }

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    std::array<float, kDecodeFrames * 2> decoded{};
    std::array<float, kDecodeFrames * 2> stereo{};

    for (;;)
    {
        const int sourceChannels = info.channels;
        const int frames = stb_vorbis_get_samples_float_interleaved(
            vorbis, sourceChannels, decoded.data(), kDecodeFrames * sourceChannels);
        if (frames <= 0)
            break;

        for (int frame = 0; frame < frames; ++frame)
        {
            const float left = decoded[static_cast<std::size_t>(frame * sourceChannels)];
            const float right = sourceChannels > 1
                ? decoded[static_cast<std::size_t>(frame * sourceChannels + 1)]
                : left;
            stereo[static_cast<std::size_t>(frame * 2)] = left;
            stereo[static_cast<std::size_t>(frame * 2 + 1)] = right;
        }

        std::size_t source = 0;
        const std::size_t sampleCount = static_cast<std::size_t>(frames) * 2u;
        while (source < sampleCount)
        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_condition.wait(lock, [] { return s_stop || s_count < kRingSamples; });
            if (s_stop)
            {
                stb_vorbis_close(vorbis);
                s_running = false;
                return;
            }

            const std::size_t freeSamples = kRingSamples - s_count;
            const std::size_t contiguous = std::min(freeSamples, kRingSamples - s_write);
            const std::size_t copyCount = std::min(contiguous, sampleCount - source);
            std::copy_n(stereo.data() + source, copyCount, s_ring.data() + s_write);
            s_write = (s_write + copyCount) % kRingSamples;
            s_count += copyCount;
            source += copyCount;
        }
    }

    stb_vorbis_close(vorbis);
    std::lock_guard<std::mutex> lock(s_mutex);
    s_eof = true;
    if (s_count == 0)
        s_running = false;
}
}

namespace PcMusicStream
{
bool start(const std::string &path)
{
    if (!validateOgg(path))
        return false;

    stop();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_read = 0;
        s_write = 0;
        s_count = 0;
        s_stop = false;
        s_eof = false;
        s_running = true;
    }
    s_thread = std::thread(decoderThread, path);
    return true;
}

void stop()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_stop = true;
    }
    s_condition.notify_all();
    if (s_thread.joinable())
        s_thread.join();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_read = 0;
        s_write = 0;
        s_count = 0;
        s_stop = false;
        s_eof = false;
        s_running = false;
    }
}

bool active()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_running || s_count > 0;
}

void mix(float *stereoOutput, int frames, float volume)
{
    if (stereoOutput == nullptr || frames <= 0 || volume <= 0.0f)
        return;

    std::lock_guard<std::mutex> lock(s_mutex);
    const std::size_t requested = static_cast<std::size_t>(frames) * 2u;
    const std::size_t available = std::min(requested, s_count);
    for (std::size_t i = 0; i < available; ++i)
    {
        stereoOutput[i] += s_ring[s_read] * volume;
        s_read = (s_read + 1) % kRingSamples;
    }
    s_count -= available;
    if (s_eof && s_count == 0)
        s_running = false;
    s_condition.notify_one();
}
}
