#pragma once

#include "platform/PlatformTuning.h"

#if PLATFORM_FAST_BLOCK_COLLISIONS

#include "java/Type.h"

struct PlatformBlockCollisionInfo
{
    bool unitCube = false;
};

const PlatformBlockCollisionInfo &platformGetBlockCollisionInfo(int_t blockId);

#endif
