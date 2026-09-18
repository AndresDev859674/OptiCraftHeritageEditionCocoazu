#pragma once

#include <cstdint>

namespace Ps2FramePacingPolicy
{
inline int fieldsPerFrame(int fieldHz, int targetFps)
{
    int fields = (fieldHz + targetFps / 2) / targetFps;
    if (fields < 1)
        fields = 1;
    if (fields > 4)
        fields = 4;
    return fields;
}

inline std::uint64_t targetPeriodUs(int targetFps)
{
    return 1000000ULL / static_cast<std::uint64_t>(targetFps);
}

inline bool shouldWaitForTarget(std::uint64_t lastPresentUs,
                                std::uint64_t nowUs,
                                std::uint64_t periodUs)
{
    if (lastPresentUs == 0)
        return true;
    return nowUs - lastPresentUs < periodUs;
}
}
