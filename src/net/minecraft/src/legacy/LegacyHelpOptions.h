#pragma once

#include "LegacyOptionsScreen.h"

class GuiButton;

class LegacyHelpOptions : public LegacyOptionsScreen
{
public:
    LegacyHelpOptions(GuiScreen *parent, GameSettings *settings,
        LegacyOptionsBackgroundMode backgroundMode = LegacyOptionsBackgroundMode::Panorama);
    void initGui() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;

protected:
    void actionPerformed(GuiButton *button) override;
};
