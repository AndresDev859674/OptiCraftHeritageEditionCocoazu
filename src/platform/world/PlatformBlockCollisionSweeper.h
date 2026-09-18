#pragma once

#include "platform/PlatformTuning.h"

#if PLATFORM_FAST_BLOCK_COLLISIONS || PLATFORM_EARLY_COLLISION_EXIT || PLATFORM_FLOAT_COLLISION_SWEEP

#include <vector>

#include "platform/world/PlatformCollisionSweep.h"

class AxisAlignedBB;
class World;

void platformCollectBlockCollisions(World *world, AxisAlignedBB *mask, std::vector<AxisAlignedBB *> &collisions);
bool platformHasBlockCollision(World *world, AxisAlignedBB *mask, std::vector<AxisAlignedBB *> &scratch);

#if PLATFORM_FLOAT_COLLISION_SWEEP
// Same block scan as platformCollectBlockCollisions, but unit cubes are emitted
// straight from their integer cell into the sweep's local float frame and never
// pass through a pooled double AxisAlignedBB. Sets the sweep origin and clears
// its boxes first.
void platformCollectBlockCollisionSweep(World *world, AxisAlignedBB *mask, PlatformCollisionSweep &sweep);
// Appends a double box rebased onto the sweep origin.
void platformAppendCollisionSweepBox(PlatformCollisionSweep &sweep, const AxisAlignedBB *box);
#endif

#endif
