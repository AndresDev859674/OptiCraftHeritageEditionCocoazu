#include "LegacyExperimentalOptions.h"

#include "LegacyGuiButton.h"
#include "LegacyOptionCheckbox.h"
#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/GuiButton.h"
#include "net/minecraft/src/Minecraft.h"

namespace
{
enum LegacyExperimentalButtonId
{
    BUTTON_SPECIAL_BLOCK = 610,
    BUTTON_ARMOR_DEFENSE_GUI = 611,
    BUTTON_BACK = 699
};
}

LegacyExperimentalOptions::LegacyExperimentalOptions(GuiScreen *parent, GameSettings *settingsValue,
    LegacyOptionsBackgroundMode backgroundModeValue)
    : LegacyOptionsScreen(parent, settingsValue, backgroundModeValue),
      specialBlockCheckbox(nullptr), armorDefenseGuiCheckbox(nullptr)
{
}

void LegacyExperimentalOptions::initGui()
{
    configureLegacyLayout(3, true, LegacyOptionsLayoutPreset::Compact);
    const int_t x = legacyLayout.contentX;
    const int_t w = legacyLayout.contentWidth;
    const int_t h = legacyLayout.rowHeight;

    specialBlockCheckbox = new LegacyOptionCheckbox(BUTTON_SPECIAL_BLOCK, x, legacyLayout.rowY(0), w, h,
        "HOMERO SIMPSON bloque", settings->specialBlock);
    // armorDefenseGuiCheckbox = new LegacyOptionCheckbox(BUTTON_ARMOR_DEFENSE_GUI, x, legacyLayout.rowY(1), w, h,
    //    "Armor Defense GUI", settings->armorDefenseGui);
    controlList.push_back(specialBlockCheckbox);
    // controlList.push_back(armorDefenseGuiCheckbox);
    controlList.push_back(new LegacyGuiButton(BUTTON_BACK, x, legacyLayout.rowY(2), w, h, "Back"));
}

void LegacyExperimentalOptions::actionPerformed(GuiButton *button)
{
    if (button == nullptr || !button->enabled || settings == nullptr)
        return;

    switch (button->id)
    {
    case BUTTON_SPECIAL_BLOCK:
        settings->specialBlock = !settings->specialBlock;
        specialBlockCheckbox->setChecked(settings->specialBlock);
        settings->saveOptions();
        return;
    case BUTTON_ARMOR_DEFENSE_GUI:
        settings->armorDefenseGui = !settings->armorDefenseGui;
        armorDefenseGuiCheckbox->setChecked(settings->armorDefenseGui);
        settings->saveOptions();
        return;
    case BUTTON_BACK:
        returnToParent();
        return;
    default:
        return;
    }
}

void LegacyExperimentalOptions::keyTyped(char_t c, int_t key)
{
    if (handleLegacyNavigationKey(key))
        return;
    LegacyOptionsScreen::keyTyped(c, key);
}

void LegacyExperimentalOptions::drawScreen(int_t mouseX, int_t mouseY, float_t partialTick)
{
    drawLegacyBackground(partialTick);
    updateLegacyPointerHover(mouseX, mouseY);
    GuiScreen::drawScreen(mouseX, mouseY, partialTick);
}
