#pragma once

#include "platform/world/PlatformBlockCollisionMath.h"

using PcLegacyCollisionBounds = PlatformCollisionBounds;

inline int pcLegacyCollisionStorageIndex(int localX, int localY, int localZ)
{
    return platformCollisionStorageIndex(localX, localY, localZ);
}

inline int pcLegacyCollisionNextSectionY(int y)
{
    return platformCollisionNextSectionY(y);
}

inline bool pcLegacyUnitCubeIntersects(const PcLegacyCollisionBounds &mask, int x, int y, int z)
{
    return platformUnitCubeIntersects(mask, x, y, z);
}

inline bool pcLegacyCanUseUnitCubeCollision(bool simpleOpaqueCube, bool specialCollisionShape)
{
    return platformCanUseUnitCubeCollision(simpleOpaqueCube, specialCollisionShape);
}
