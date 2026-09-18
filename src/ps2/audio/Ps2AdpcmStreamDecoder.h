#pragma once

#include <cstddef>
#include <cstdint>

namespace Ps2AdpcmStream
{
constexpr int kHeaderBytes = 16;
constexpr int kBlockBytes = 16;
constexpr int kSamplesPerBlock = 28;

struct Header
{
    int version = 0;
    int channels = 0;
    bool loop = false;
    int sampleRate = 0;
    std::uint32_t samplesPerChannel = 0;
};

struct ChannelState
{
    std::int32_t history1 = 0;
    std::int32_t history2 = 0;
};

bool parseHeader(const std::uint8_t *data, std::size_t size, Header &out);
bool decodeBlock(const std::uint8_t *block, ChannelState &state, std::int16_t *outSamples);
}
