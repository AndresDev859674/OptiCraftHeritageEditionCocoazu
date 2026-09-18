#pragma once

#include <string>

#include "java/Type.h"

class Minecraft;

// Legacy Console-style tip window: a dark rounded panel in the top right of
// the screen, shown for a few seconds. Tips queue up and show one after
// another. Only drawn on the Legacy UI, over the in-game HUD.
class LegacyTipHud
{
public:
    static void show(const std::string &text);
    static void clear();
    // Called once per game tick from GuiIngame::updateTick().
    static void tick();
    static void render(Minecraft *mc, int_t screenWidth, int_t screenHeight);
};
