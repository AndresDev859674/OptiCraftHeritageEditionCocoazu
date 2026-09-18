#pragma once

#include <cstddef>

#include "ps2/tuning/Ps2MeshTuning.h"

namespace Ps2MeshMemoryPolicy
{
enum class PressureLevel
{
    None,
    Soft,
    Hard
};

inline PressureLevel pressureLevel(std::size_t meshBytes, unsigned long freeKb)
{
    if (meshBytes >= PS2_MESH_RAM_HARD_BYTES || freeKb <= PS2_MESH_RAM_EMERGENCY_FREE_KB)
        return PressureLevel::Hard;
    if (meshBytes >= PS2_MESH_RAM_TARGET_BYTES)
        return PressureLevel::Soft;
    return PressureLevel::None;
}

inline int maxEvictionsPerFrame(PressureLevel level)
{
    if (level == PressureLevel::Hard)
        return PS2_MESH_TRIM_HARD_MAX_PER_FRAME;
    if (level == PressureLevel::Soft)
        return PS2_MESH_TRIM_SOFT_MAX_PER_FRAME;
    return 0;
}

inline bool shouldSkipOffscreenRebuilds(PressureLevel level)
{
    return level != PressureLevel::None;
}

inline bool canEvictRenderer(bool initialized, bool buildActive, bool inFrustum, float distanceSq)
{
    const float keepSq = PS2_MESH_TRIM_KEEP_RADIUS_BLOCKS * PS2_MESH_TRIM_KEEP_RADIUS_BLOCKS;
    return initialized && !buildActive && !inFrustum && distanceSq > keepSq;
}
}
