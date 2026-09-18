#pragma once

#include <array>
#include <cstdint>

namespace Ps2SectionOcclusion
{
    static constexpr int kFaceCount = 6;
    static constexpr int kMaxSections = 128;
    static constexpr std::uint8_t kAllFaces = (1u << kFaceCount) - 1u;

    enum Face : int
    {
        Down = 0,
        Up = 1,
        North = 2,
        South = 3,
        West = 4,
        East = 5
    };

    void compute(int sectionCount,
        int cameraIndex,
        const std::array<std::int16_t, kFaceCount> *neighbours,
        const std::array<std::uint8_t, kFaceCount> *visibleFaces,
        bool *outVisible);
}
