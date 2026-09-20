#pragma once

#include "LegacyOptionsScreen.h"
#include "platform/PlatformConfig.h"

class GuiButton;
class LegacyOptionCheckbox;
class LegacyOptionSlider;

class LegacyVideoOptions : public LegacyOptionsScreen
{
public:
    LegacyVideoOptions(GuiScreen *parent, GameSettings *settings,
        LegacyOptionsBackgroundMode backgroundMode = LegacyOptionsBackgroundMode::Panorama);
    void initGui() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;

protected:
    void actionPerformed(GuiButton *button) override;

private:
    void syncCheckboxes();
#if !(PLATFORM_PS2 || PLATFORM_WII)
    void rebuildPage();
#endif

    LegacyOptionCheckbox *graphicsCheckbox;
    LegacyOptionCheckbox *smoothLightingCheckbox;
    LegacyOptionCheckbox *viewBobbingCheckbox;
    LegacyOptionCheckbox *cloudsCheckbox;
    LegacyOptionCheckbox *fogCheckbox;
    // Wii only: the EFB->XFB deflicker filter; null elsewhere.
    LegacyOptionCheckbox *deflickerCheckbox;
#if !(PLATFORM_PS2 || PLATFORM_WII)
    int_t currentPage;
#endif
};
