#pragma once

#include <cstdint>

struct PcLegacyHudStatusState
{
    int health = 20;
    int prevHealth = 20;
    int armor = 0;
    int foodLevel = 20;
    int air = 300;
    int xpCap = 0;
    int xpFilled = 0;
    bool saturationPositive = true;
    bool underwater = false;
    bool poisoned = false;
    bool hungry = false;
    bool hardcore = false;
    bool flashHearts = false;
    bool regeneration = false;
};

inline bool pcLegacyCanCacheHudStatus(const PcLegacyHudStatusState &state)
{
    return state.health > 4 && !state.regeneration && !state.flashHearts && state.saturationPositive;
}

inline std::uint64_t pcLegacyHudStatusSignature(const PcLegacyHudStatusState &state)
{
    std::uint64_t value = 0;
    value |= static_cast<std::uint64_t>(state.health & 31);
    value |= static_cast<std::uint64_t>(state.prevHealth & 31) << 5;
    value |= static_cast<std::uint64_t>(state.armor & 31) << 10;
    value |= static_cast<std::uint64_t>(state.foodLevel & 31) << 15;
    value |= static_cast<std::uint64_t>(state.air & 511) << 20;
    value |= static_cast<std::uint64_t>(state.saturationPositive) << 29;
    value |= static_cast<std::uint64_t>(state.underwater) << 30;
    value |= static_cast<std::uint64_t>(state.poisoned) << 31;
    value |= static_cast<std::uint64_t>(state.hungry) << 32;
    value |= static_cast<std::uint64_t>(state.hardcore) << 33;
    value |= static_cast<std::uint64_t>(state.flashHearts) << 34;
    value |= static_cast<std::uint64_t>(state.regeneration) << 35;
    value ^= static_cast<std::uint64_t>(state.xpCap & 0xffff) << 36;
    value ^= static_cast<std::uint64_t>(state.xpFilled & 0xff) << 52;
    return value;
}
