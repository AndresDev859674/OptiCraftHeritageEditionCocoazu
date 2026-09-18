#pragma once

#include <cstdint>

#include "java/Type.h"

struct WiiBlockRenderInfo
{
    std::uint8_t renderPass = 0;
    bool simpleOpaqueCube = false;
};

constexpr bool wiiSimpleOpaqueCubeEligible(int renderType, int renderPass,
    bool opaqueCube, bool normalCube, bool defaultFaceCulling, bool unitBounds)
{
    return renderType == 0 && renderPass == 0 && opaqueCube && normalCube &&
        defaultFaceCulling && unitBounds;
}

const WiiBlockRenderInfo &wiiGetBlockRenderInfo(int_t blockId);
