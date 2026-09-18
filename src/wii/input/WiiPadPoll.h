#pragma once
#ifdef WII_PLATFORM

#include <cstdint>

void wiiPadPoll(std::uint32_t gameCubeConnected, bool inMenu, bool specializedMenuNavigation);

#endif // WII_PLATFORM
