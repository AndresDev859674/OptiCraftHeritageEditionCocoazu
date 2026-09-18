#include "ps2/audio/Ps2AdpcmStreamDecoder.h"

#include <algorithm>

namespace
{
constexpr int kPredictor1[5] = {0, 60, 115, 98, 122};
constexpr int kPredictor2[5] = {0, 0, -52, -55, -60};

struct SupportedSampleRate
{
    int rate;
    bool mono;
    bool stereo;
};

constexpr SupportedSampleRate kSupportedSampleRates[] = {
    {11025, true, true},
    {12000, false, true},
    {22050, true, true},
    {24000, false, true},
    {32000, true, true},
    {44100, true, true},
    {48000, true, true}
};

int sampleRateFromPitch(std::uint32_t pitch, int channels)
{
    int bestRate = 0;
    std::uint32_t bestDifference = 0xffffffffu;

    for (const SupportedSampleRate &candidate : kSupportedSampleRates)
    {
        if ((channels == 1 && !candidate.mono) || (channels == 2 && !candidate.stereo))
            continue;

        const std::uint32_t expectedPitch =
            static_cast<std::uint32_t>((static_cast<std::uint64_t>(candidate.rate) * 4096u) / 48000u);
        const std::uint32_t difference = pitch > expectedPitch ? pitch - expectedPitch : expectedPitch - pitch;
        if (difference < bestDifference)
        {
            bestDifference = difference;
            bestRate = candidate.rate;
        }
    }

    return bestDifference <= 1u ? bestRate : 0;
}

std::uint32_t readLe32(const std::uint8_t *data)
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::int32_t arithmeticShiftRight(std::int64_t value, int shift)
{
    if (shift <= 0)
        return static_cast<std::int32_t>(value);
    if (value >= 0)
        return static_cast<std::int32_t>(value >> shift);

    const std::uint64_t magnitude = static_cast<std::uint64_t>(-value);
    const std::uint64_t rounded = (magnitude + ((std::uint64_t{1} << shift) - 1u)) >> shift;
    return -static_cast<std::int32_t>(rounded);
}

std::int16_t clampSample(std::int32_t sample)
{
    sample = std::max<std::int32_t>(-32768, std::min<std::int32_t>(32767, sample));
    return static_cast<std::int16_t>(sample);
}
}

namespace Ps2AdpcmStream
{
bool parseHeader(const std::uint8_t *data, std::size_t size, Header &out)
{
    if (data == nullptr || size < kHeaderBytes ||
        data[0] != 'A' || data[1] != 'P' || data[2] != 'C' || data[3] != 'M')
    {
        return false;
    }

    const int channels = static_cast<int>(data[5]);
    const std::uint32_t pitch = readLe32(data + 8);
    const std::uint32_t samples = readLe32(data + 12);
    if ((channels != 1 && channels != 2) || pitch == 0 || samples == 0)
        return false;

    const int sampleRate = sampleRateFromPitch(pitch, channels);
    if (sampleRate == 0)
        return false;

    out.version = static_cast<int>(data[4]);
    out.channels = channels;
    out.loop = data[6] != 0;
    out.sampleRate = sampleRate;
    out.samplesPerChannel = samples;
    return true;
}

bool decodeBlock(const std::uint8_t *block, ChannelState &state, std::int16_t *outSamples)
{
    if (block == nullptr || outSamples == nullptr)
        return false;

    const int predictor = static_cast<int>((block[0] >> 4) & 0x0f);
    const int shift = static_cast<int>(block[0] & 0x0f);
    if (predictor < 0 || predictor >= 5 || shift > 12)
        return false;

    int outputIndex = 0;
    for (int byteIndex = 2; byteIndex < kBlockBytes; ++byteIndex)
    {
        const std::uint8_t packed = block[byteIndex];
        for (int nibbleIndex = 0; nibbleIndex < 2; ++nibbleIndex)
        {
            int nibble = nibbleIndex == 0 ? (packed & 0x0f) : ((packed >> 4) & 0x0f);
            if ((nibble & 0x08) != 0)
                nibble -= 16;

            const std::int32_t residual = arithmeticShiftRight(
                static_cast<std::int64_t>(nibble) << 12, shift);
            const std::int64_t predictorValue =
                static_cast<std::int64_t>(state.history1) * kPredictor1[predictor] +
                static_cast<std::int64_t>(state.history2) * kPredictor2[predictor] + 32;
            const std::int32_t prediction = arithmeticShiftRight(predictorValue, 6);
            const std::int16_t sample = clampSample(residual + prediction);

            state.history2 = state.history1;
            state.history1 = sample;
            outSamples[outputIndex++] = sample;
        }
    }

    return outputIndex == kSamplesPerBlock;
}
}
