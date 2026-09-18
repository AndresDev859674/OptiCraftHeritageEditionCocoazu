#pragma once

#include <string>
#include "java/Type.h"

class Minecraft;

inline std::string legacyFormatControlPrompt(const std::string &button, const std::string &action)
{
    if (button.empty())
        return action;
    return "[" + button + "] " + action;
}

class LegacyControlTooltipHud
{
public:
    static void render(Minecraft *mc, int_t screenWidth, int_t screenHeight);
};
