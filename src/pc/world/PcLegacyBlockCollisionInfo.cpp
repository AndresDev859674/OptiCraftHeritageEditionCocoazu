#include "pc/world/PcLegacyBlockCollisionInfo.h"

#if PLATFORM_PC_LEGACY

const PcLegacyBlockCollisionInfo &pcLegacyGetBlockCollisionInfo(int_t blockId)
{
    return platformGetBlockCollisionInfo(blockId);
}

#endif
