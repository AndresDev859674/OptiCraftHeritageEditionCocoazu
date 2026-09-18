#pragma once

#include <algorithm>
#include <vector>

#include "java/Type.h"
#include "platform/PlatformTuning.h"

namespace ChunkMemoryPolicy
{
struct RetentionPolicy
{
    int_t loadRadius;
    int_t unloadRadius;
    int_t maxUnloadsPerTick;
    long_t minUnusedTicksBeforeUnload;
};

inline RetentionPolicy retentionPolicy(bool readOnly)
{
    if (readOnly)
    {
        return {
            PLATFORM_READ_ONLY_CHUNK_CACHE_RADIUS,
            PLATFORM_READ_ONLY_CHUNK_UNLOAD_RADIUS,
            PLATFORM_READ_ONLY_MAX_CHUNK_UNLOADS_PER_TICK,
            PLATFORM_READ_ONLY_MIN_UNUSED_TICKS_BEFORE_UNLOAD
        };
    }

    return {
        PLATFORM_CHUNK_CACHE_RADIUS,
        PLATFORM_CHUNK_UNLOAD_RADIUS,
        PLATFORM_MAX_CHUNK_UNLOADS_PER_TICK,
        PLATFORM_MIN_UNUSED_TICKS_BEFORE_UNLOAD
    };
}

inline int_t unloadLimit(const RetentionPolicy &policy, bool emergency)
{
    return emergency
        ? std::max<int_t>(PLATFORM_EMERGENCY_CHUNK_UNLOADS_PER_TICK, policy.maxUnloadsPerTick)
        : policy.maxUnloadsPerTick;
}

inline bool hasOnlyPackedNibbleValue(const std::vector<byte_t> &data, byte_t value)
{
    return !data.empty() && std::all_of(data.begin(), data.end(), [value](byte_t current)
    {
        return current == value;
    });
}

inline bool canDiscardEmptyAnvilSection(int_t blockCount, bool hasSky,
                                        const std::vector<byte_t> &blockLight,
                                        const std::vector<byte_t> &skyLight)
{
    constexpr std::size_t sectionNibbleBytes = 16u * 16u * 16u / 2u;
    if (blockCount != 0 || blockLight.size() != sectionNibbleBytes
        || !hasOnlyPackedNibbleValue(blockLight, (byte_t)0x00))
    {
        return false;
    }

    return !hasSky || (skyLight.size() == sectionNibbleBytes
        && hasOnlyPackedNibbleValue(skyLight, (byte_t)0xff));
}
}
