#pragma once

#include "LegacyMainMenuLayout.h"

class Minecraft;

bool legacyDrawTitleTexture(Minecraft *mc, const LegacyMainMenuLayout &layout, int_t screenWidth,
    float_t zLevel, LegacyUiRect *outRect);
