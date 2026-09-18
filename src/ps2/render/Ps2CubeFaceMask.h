#pragma once

#include <cstdint>

namespace Ps2CubeFaceMask
{
    enum Face : int
    {
        Down = 0,
        Up = 1,
        North = 2,
        South = 3,
        West = 4,
        East = 5
    };

    constexpr std::uint8_t kAllFaces = 0x3fu;

    constexpr std::uint8_t exposedFromOpaque(std::uint8_t opaqueNeighbours)
    {
        return static_cast<std::uint8_t>((~opaqueNeighbours) & kAllFaces);
    }
}
