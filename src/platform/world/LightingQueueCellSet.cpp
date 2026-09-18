#include "platform/world/LightingQueueCellSet.h"

#include <cstdint>

#include "net/minecraft/src/EnumSkyBlock.h"

namespace
{
    // Bit 63 marks an occupied slot, so a genuine key of zero is distinguishable
    // from an empty one. x and z keep 26 bits each (the world is bounded at
    // +-30,000,000), y 7 bits, type 1 bit.
    constexpr ulong_t OCCUPIED_BIT = 1ULL << 63;
    constexpr ulong_t AXIS_MASK = (1ULL << 26) - 1;
}

ulong_t LightingQueueCellSet::key(const EnumSkyBlock *type, int_t x, int_t y, int_t z)
{
    const ulong_t typeBit = type == EnumSkyBlock::Sky ? 1ULL : 0ULL;
    return OCCUPIED_BIT |
        ((static_cast<ulong_t>(x) & AXIS_MASK) << 34) |
        ((static_cast<ulong_t>(z) & AXIS_MASK) << 8) |
        ((static_cast<ulong_t>(y) & 0x7fULL) << 1) |
        typeBit;
}

std::size_t LightingQueueCellSet::slotOf(ulong_t cellKey)
{
    // Folded to 32 bits before the multiply: a 64-bit product is a library call
    // on the EE, and the hash only needs to spread neighbouring cells apart.
    const std::uint32_t folded = static_cast<std::uint32_t>(cellKey) ^
                                 static_cast<std::uint32_t>(cellKey >> 32);
    const std::uint32_t mixed = folded * 0x9E3779B1u;
    return static_cast<std::size_t>(mixed >> 20) & (SLOT_COUNT - 1);
}

bool LightingQueueCellSet::insert(ulong_t cellKey)
{
    ulong_t &slot = slots[slotOf(cellKey)];
    if (slot == cellKey)
        return false;
    slot = cellKey;
    return true;
}

void LightingQueueCellSet::erase(ulong_t cellKey)
{
    ulong_t &slot = slots[slotOf(cellKey)];
    if (slot == cellKey)
        slot = 0;
}

void LightingQueueCellSet::clear()
{
    slots.fill(0);
}
