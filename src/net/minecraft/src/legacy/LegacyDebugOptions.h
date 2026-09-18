#pragma once

#include "LegacyOptionsScreen.h"

class GuiButton;
class LegacyGuiButton;
class LegacyOptionCheckbox;

class LegacyDebugOptions : public LegacyOptionsScreen
{
public:
    LegacyDebugOptions(GuiScreen *parent, GameSettings *settings,
        LegacyOptionsBackgroundMode backgroundMode = LegacyOptionsBackgroundMode::PausedWorld);

    void initGui() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;

protected:
    void actionPerformed(GuiButton *button) override;
    void keyTyped(char_t c, int_t key) override;

private:
    void syncControls();
    void setCreativeMode(bool creative);
    void setDay();
    void killEntities();

    LegacyOptionCheckbox *showFpsCheckbox;
    LegacyOptionCheckbox *extendedInfoCheckbox;
    LegacyOptionCheckbox *keepInventoryCheckbox;
    LegacyGuiButton *setDayButton;
    LegacyGuiButton *gameModeButton;
    LegacyGuiButton *killEntitiesButton;
    bool multiplayer;
};
