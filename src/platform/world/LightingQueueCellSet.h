#pragma once

#include <array>
#include <cstddef>

#include "java/Type.h"

class EnumSkyBlock;

// Direct-mapped set of the single-cell jobs currently sitting in
// World::lightingToUpdate.
//
// Propagation schedules every neighbour whose light disagrees, and each of those
// requests used to be deduplicated by scanning the tail of the queue with
// MetadataChunkBlock::tryMerge -- PLATFORM_LIGHTING_MERGE_SCAN entries per
// request, up to six requests per cell that changed. This answers the same
// question ("is this cell already queued?") in one load.
//
// A slot holds at most one key. Inserting over a different key evicts it, which
// only means that cell may be queued twice later -- the same outcome as a scan
// miss, and harmless because a lighting job is a recomputation. Never a false
// positive: a slot only reports present when the full key matches.
class LightingQueueCellSet
{
public:
    static constexpr std::size_t SLOT_COUNT = 4096;

    static ulong_t key(const EnumSkyBlock *type, int_t x, int_t y, int_t z);

    // Returns false when the cell was already present.
    bool insert(ulong_t cellKey);
    void erase(ulong_t cellKey);
    void clear();

private:
    static std::size_t slotOf(ulong_t cellKey);

    std::array<ulong_t, SLOT_COUNT> slots{};
};
