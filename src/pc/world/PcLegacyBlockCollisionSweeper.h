#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_PC_LEGACY

#include <vector>

class AxisAlignedBB;
class World;

void pcLegacyCollectBlockCollisions(World *world, AxisAlignedBB *mask, std::vector<AxisAlignedBB *> &collisions);
bool pcLegacyHasBlockCollision(World *world, AxisAlignedBB *mask, std::vector<AxisAlignedBB *> &scratch);

#endif
