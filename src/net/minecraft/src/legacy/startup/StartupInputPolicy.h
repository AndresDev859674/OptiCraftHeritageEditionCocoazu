#pragma once

#include "platform/Input.h"

#include <cstdint>

namespace LegacyStartup
{

inline bool shouldSkipForTextActions(std::uint32_t heldActions)
{
    return (heldActions & (PLATFORM_TEXT_TYPE | PLATFORM_TEXT_ENTER)) != 0;
}

} // namespace LegacyStartup
