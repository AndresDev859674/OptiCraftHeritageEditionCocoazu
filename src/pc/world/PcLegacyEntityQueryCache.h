#pragma once

#include "platform/world/PlatformEntityQueryCache.h"

template <typename T, std::size_t SlotCount = 6>
using PcLegacyEntityQueryCache = PlatformEntityQueryCache<T, SlotCount>;
