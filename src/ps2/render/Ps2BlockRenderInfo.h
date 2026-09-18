#pragma once

#include <array>
#include <cstdint>

#include "java/Type.h"

struct Ps2BlockRenderInfo
{
    std::int8_t renderType = -1;
    std::uint8_t renderPass = 0;
    bool simpleOpaqueCube = false;
    bool staticTextureBySide = false;
    bool defaultWhiteColorMultiplier = false;
    std::array<std::int16_t, 6> textureBySide{};
};

constexpr bool ps2SimpleOpaqueCubeEligible(int renderType, int renderPass,
    bool opaqueCube, bool normalCube, bool defaultFaceCulling, bool unitBounds)
{
    return renderType == 0 && renderPass == 0 && opaqueCube && normalCube &&
        defaultFaceCulling && unitBounds;
}

const Ps2BlockRenderInfo &ps2GetBlockRenderInfo(int_t blockId);
