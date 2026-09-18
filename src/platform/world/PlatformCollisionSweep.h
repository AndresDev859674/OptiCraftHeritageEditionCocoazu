#pragma once

#include "platform/PlatformTuning.h"

#if PLATFORM_FLOAT_COLLISION_SWEEP

#include <vector>

#include "java/Type.h"

// Collision candidates for one Entity::moveEntity sweep, as float boxes in a
// local frame whose origin is an integer block corner next to the entity.
//
// Block boxes are born as an integer cell plus 1/16-step bounds and every
// candidate lies within a few blocks of the origin, so float represents them
// with more resolution than the sweep can observe, while the R5900 runs the
// whole scan on its FPU instead of through libgcc double emulation. The
// entity's own AxisAlignedBB stays double; only the per-box work moves.
struct PlatformCollisionSweepBox
{
    float minX;
    float minY;
    float minZ;
    float maxX;
    float maxY;
    float maxZ;
};

struct PlatformCollisionSweep
{
    int_t originX = 0;
    int_t originY = 0;
    int_t originZ = 0;
    std::vector<PlatformCollisionSweepBox> boxes;
};

#endif // PLATFORM_FLOAT_COLLISION_SWEEP
