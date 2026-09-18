#include "pc/world/PcLegacyBlockCollisionSweeper.h"

#if PLATFORM_PC_LEGACY

#include "platform/world/PlatformBlockCollisionSweeper.h"

void pcLegacyCollectBlockCollisions(World *world, AxisAlignedBB *mask, std::vector<AxisAlignedBB *> &collisions)
{
    platformCollectBlockCollisions(world, mask, collisions);
}

bool pcLegacyHasBlockCollision(World *world, AxisAlignedBB *mask, std::vector<AxisAlignedBB *> &scratch)
{
    return platformHasBlockCollision(world, mask, scratch);
}

#endif
