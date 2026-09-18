#pragma once

#include "LegacyLanguageModel.h"
#include "LegacyOptionsScreen.h"

class GuiButton;

class LegacyLanguageOptions : public LegacyOptionsScreen
{
public:
    LegacyLanguageOptions(GuiScreen *parent, GameSettings *settings,
        LegacyOptionsBackgroundMode backgroundMode = LegacyOptionsBackgroundMode::Panorama);

    void initGui() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;

protected:
    void actionPerformed(GuiButton *button) override;

private:
    void rebuildButtons();
    void applyLanguage(const LegacyLanguageEntry &entry);

    LegacyLanguageModel languageModel;
    int_t currentPage;
    int_t visibleRows;
};
