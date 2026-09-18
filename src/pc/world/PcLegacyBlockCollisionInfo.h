#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_PC_LEGACY

#include "platform/world/PlatformBlockCollisionInfo.h"

using PcLegacyBlockCollisionInfo = PlatformBlockCollisionInfo;

const PcLegacyBlockCollisionInfo &pcLegacyGetBlockCollisionInfo(int_t blockId);

#endif
