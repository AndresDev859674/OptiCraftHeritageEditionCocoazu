#pragma once

#include "LegacyOptionsScreen.h"

class GuiButton;
class LegacyOptionCheckbox;

class LegacyExperimentalOptions : public LegacyOptionsScreen
{
public:
    LegacyExperimentalOptions(GuiScreen *parent, GameSettings *settings,
        LegacyOptionsBackgroundMode backgroundMode = LegacyOptionsBackgroundMode::Panorama);

    void initGui() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;

protected:
    void keyTyped(char_t c, int_t key) override;
    void actionPerformed(GuiButton *button) override;

private:
    LegacyOptionCheckbox *specialBlockCheckbox;
    LegacyOptionCheckbox *armorDefenseGuiCheckbox;
};
