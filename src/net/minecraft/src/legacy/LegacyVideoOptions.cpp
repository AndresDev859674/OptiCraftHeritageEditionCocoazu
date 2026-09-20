#include "LegacyVideoOptions.h"

#include <algorithm>

#include "LegacyGuiButton.h"
#include "LegacyOptionCheckbox.h"
#include "LegacyOptionSlider.h"
#include "LegacyOptionState.h"
#include "LegacyOptionMetrics.h"
#include "net/minecraft/src/EnumOptions.h"
#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/GuiButton.h"
#include "net/minecraft/src/GuiAnimationSettingsOF.h"
#include "net/minecraft/src/GuiDetailSettingsOF.h"
#include "net/minecraft/src/GuiOtherSettingsOF.h"
#include "net/minecraft/src/GuiPerformanceSettingsOF.h"
#include "net/minecraft/src/GuiQualitySettingsOF.h"
#include "net/minecraft/src/Minecraft.h"
#include "platform/PlatformConfig.h"
#include "platform/PlatformUserSettings.h"

namespace
{
enum LegacyVideoButtonId
{
    BUTTON_RENDER_DISTANCE = 300,
    BUTTON_GRAPHICS = 301,
    BUTTON_SMOOTH_LIGHTING = 302,
    BUTTON_VIEW_BOBBING = 303,
    BUTTON_CLOUDS = 304,
    BUTTON_FOG = 305,
    BUTTON_BRIGHTNESS = 306,
    BUTTON_DEFLICKER = 307,
    BUTTON_FRAMERATE_LIMIT = 308,
    BUTTON_ANIMATIONS = 309,
    BUTTON_QUALITY = 310,
    BUTTON_PREVIOUS = 311,
    BUTTON_NEXT = 312,
    BUTTON_DETAILS = 313,
    BUTTON_PERFORMANCE = 314,
    BUTTON_OTHER = 315,
    BUTTON_DONE = 399
};

std::string legacyFramerateLabel(const GameSettings *settings)
{
    static const char *labels[] = { "Maximum", "Balanced", "Power Saver" };
    const int_t value = settings != nullptr ? settings->limitFramerate : 1;
    const int_t index = value >= 0 && value < 3 ? value : 1;
    return std::string("Framerate Limit: ") + labels[index];
}

}

LegacyVideoOptions::LegacyVideoOptions(GuiScreen *parent, GameSettings *settingsValue,
    LegacyOptionsBackgroundMode backgroundModeValue)
    : LegacyOptionsScreen(parent, settingsValue, backgroundModeValue), graphicsCheckbox(nullptr), smoothLightingCheckbox(nullptr),
      viewBobbingCheckbox(nullptr), cloudsCheckbox(nullptr), fogCheckbox(nullptr), deflickerCheckbox(nullptr)
#if !(PLATFORM_PS2 || PLATFORM_WII)
    , currentPage(0)
#endif
{
}

void LegacyVideoOptions::initGui()
{
#if PLATFORM_WII
    const int_t rowCount = 7;
#elif PLATFORM_PS2
    const int_t rowCount = 6;
#else
    const int_t rowCount = 8;
#endif
    configureLegacyLayout(rowCount, true, LegacyOptionsLayoutPreset::Compact);
#if !(PLATFORM_PS2 || PLATFORM_WII)
    rebuildPage();
    return;
#endif
    const int_t x = legacyLayout.contentX;
    const int_t w = legacyLayout.contentWidth;
    const int_t h = legacyLayout.rowHeight;
    int_t row = 0;

    graphicsCheckbox = new LegacyOptionCheckbox(BUTTON_GRAPHICS, x, legacyLayout.rowY(row++), w, h,
        "Fancy Graphics", settings->fancyGraphics);
    smoothLightingCheckbox = new LegacyOptionCheckbox(BUTTON_SMOOTH_LIGHTING, x, legacyLayout.rowY(row++), w, h,
        "Smooth Lighting", legacySmoothLightingChecked(settings->ofAoLevel));
    viewBobbingCheckbox = new LegacyOptionCheckbox(BUTTON_VIEW_BOBBING, x, legacyLayout.rowY(row++), w, h,
        "View Bobbing", settings->viewBobbing);

    controlList.push_back(graphicsCheckbox);
    controlList.push_back(smoothLightingCheckbox);
    controlList.push_back(viewBobbingCheckbox);

#if !(PLATFORM_PS2 || PLATFORM_WII)
    cloudsCheckbox = new LegacyOptionCheckbox(BUTTON_CLOUDS, x, legacyLayout.rowY(row++), w, h,
        "Render Clouds", legacyCloudsChecked(settings->ofClouds));
    fogCheckbox = new LegacyOptionCheckbox(BUTTON_FOG, x, legacyLayout.rowY(row++), w, h,
        "Fog", legacyFogChecked(settings->ofFogOff));
    controlList.push_back(cloudsCheckbox);
    controlList.push_back(fogCheckbox);
#else
    cloudsCheckbox = nullptr;
    fogCheckbox = nullptr;
#endif

#if PLATFORM_WII
    // The vertical copy filter is what reads as "anti-aliasing" on the Wii:
    // it steadies a 480i picture at the cost of a vertical blur.
    deflickerCheckbox = new LegacyOptionCheckbox(BUTTON_DEFLICKER, x, legacyLayout.rowY(row++), w, h,
        "Deflicker Filter", settings->wiiDeflicker);
    controlList.push_back(deflickerCheckbox);
#else
    deflickerCheckbox = nullptr;
#endif

    controlList.push_back(new LegacyOptionSlider(BUTTON_RENDER_DISTANCE, x, legacyLayout.rowY(row++), w, h,
        settings, EnumOptions::RENDER_DISTANCE_FINE));
    controlList.push_back(new LegacyOptionSlider(BUTTON_BRIGHTNESS, x, legacyLayout.rowY(row++), w, h,
        settings, EnumOptions::BRIGHTNESS));
    controlList.push_back(new LegacyGuiButton(BUTTON_DONE, x, legacyLayout.rowY(row), w, h, "Done"));
}

#if !(PLATFORM_PS2 || PLATFORM_WII)
void LegacyVideoOptions::rebuildPage()
{
    controlList.clear();
    graphicsCheckbox = nullptr;
    smoothLightingCheckbox = nullptr;
    viewBobbingCheckbox = nullptr;
    cloudsCheckbox = nullptr;
    fogCheckbox = nullptr;

    const int_t x = legacyLayout.contentX;
    const int_t w = legacyLayout.contentWidth;
    const int_t h = legacyLayout.rowHeight;
    int_t row = 0;

    if (currentPage == 0)
    {
        graphicsCheckbox = new LegacyOptionCheckbox(BUTTON_GRAPHICS, x, legacyLayout.rowY(row++), w, h,
            "Fancy Graphics", settings->fancyGraphics);
        smoothLightingCheckbox = new LegacyOptionCheckbox(BUTTON_SMOOTH_LIGHTING, x, legacyLayout.rowY(row++), w, h,
            "Smooth Lighting", legacySmoothLightingChecked(settings->ofAoLevel));
        viewBobbingCheckbox = new LegacyOptionCheckbox(BUTTON_VIEW_BOBBING, x, legacyLayout.rowY(row++), w, h,
            "View Bobbing", settings->viewBobbing);
        cloudsCheckbox = new LegacyOptionCheckbox(BUTTON_CLOUDS, x, legacyLayout.rowY(row++), w, h,
            "Render Clouds", legacyCloudsChecked(settings->ofClouds));
        fogCheckbox = new LegacyOptionCheckbox(BUTTON_FOG, x, legacyLayout.rowY(row++), w, h,
            "Fog", legacyFogChecked(settings->ofFogOff));

        controlList.push_back(graphicsCheckbox);
        controlList.push_back(smoothLightingCheckbox);
        controlList.push_back(viewBobbingCheckbox);
        controlList.push_back(cloudsCheckbox);
        controlList.push_back(fogCheckbox);
        controlList.push_back(new LegacyOptionSlider(BUTTON_RENDER_DISTANCE, x, legacyLayout.rowY(row++), w, h,
            settings, EnumOptions::RENDER_DISTANCE_FINE));
    }
    else if (currentPage == 1)
    {
        controlList.push_back(new LegacyOptionSlider(BUTTON_BRIGHTNESS, x, legacyLayout.rowY(row++), w, h,
            settings, EnumOptions::BRIGHTNESS));
        controlList.push_back(new LegacyGuiButton(BUTTON_FRAMERATE_LIMIT, x, legacyLayout.rowY(row++), w, h,
            legacyFramerateLabel(settings)));
        controlList.push_back(new LegacyGuiButton(BUTTON_ANIMATIONS, x, legacyLayout.rowY(row++), w, h,
            "Animations..."));
        controlList.push_back(new LegacyGuiButton(BUTTON_QUALITY, x, legacyLayout.rowY(row++), w, h,
            "Quality..."));
    }
    else
    {
        controlList.push_back(new LegacyGuiButton(BUTTON_DETAILS, x, legacyLayout.rowY(row++), w, h,
            "Details..."));
        controlList.push_back(new LegacyGuiButton(BUTTON_PERFORMANCE, x, legacyLayout.rowY(row++), w, h,
            "Performance..."));
        controlList.push_back(new LegacyGuiButton(BUTTON_OTHER, x, legacyLayout.rowY(row++), w, h,
            "Other..."));
    }

    const int_t navY = legacyLayout.rowY(6);
    const int_t gap = 2;
    const int_t halfWidth = (w - gap) / 2;
    LegacyGuiButton *previous = new LegacyGuiButton(BUTTON_PREVIOUS, x, navY, halfWidth, h, "Previous");
    LegacyGuiButton *next = new LegacyGuiButton(BUTTON_NEXT, x + halfWidth + gap, navY,
        w - halfWidth - gap, h, "Next");
    previous->enabled = currentPage > 0;
    next->enabled = currentPage < 2;
    controlList.push_back(previous);
    controlList.push_back(next);
    controlList.push_back(new LegacyGuiButton(BUTTON_DONE, x, legacyLayout.rowY(7), w, h, "Done"));
}
#endif

void LegacyVideoOptions::syncCheckboxes()
{
    graphicsCheckbox->setChecked(settings->fancyGraphics);
    smoothLightingCheckbox->setChecked(legacySmoothLightingChecked(settings->ofAoLevel));
    viewBobbingCheckbox->setChecked(settings->viewBobbing);
    if (cloudsCheckbox != nullptr)
        cloudsCheckbox->setChecked(legacyCloudsChecked(settings->ofClouds));
    if (fogCheckbox != nullptr)
        fogCheckbox->setChecked(legacyFogChecked(settings->ofFogOff));
    if (deflickerCheckbox != nullptr)
        deflickerCheckbox->setChecked(settings->wiiDeflicker);
}

void LegacyVideoOptions::actionPerformed(GuiButton *button)
{
    if (button == nullptr || !button->enabled)
        return;

#if !(PLATFORM_PS2 || PLATFORM_WII)
    if (button->id == BUTTON_PREVIOUS)
    {
        currentPage = std::max<int_t>(0, currentPage - 1);
        rebuildPage();
        return;
    }
    if (button->id == BUTTON_NEXT)
    {
        currentPage = std::min<int_t>(2, currentPage + 1);
        rebuildPage();
        return;
    }
#endif

    switch (button->id)
    {
    case BUTTON_GRAPHICS:
        settings->setOptionValue(EnumOptions::GRAPHICS, 1);
        syncCheckboxes();
        return;
    case BUTTON_SMOOTH_LIGHTING:
        settings->setOptionFloatValue(EnumOptions::AO_LEVEL,
            legacySmoothLightingToggleValue(settings->ofAoLevel));
        syncCheckboxes();
        return;
    case BUTTON_VIEW_BOBBING:
        settings->setOptionValue(EnumOptions::VIEW_BOBBING, 1);
        syncCheckboxes();
        return;
    case BUTTON_CLOUDS:
        settings->ofClouds = legacyCloudsToggledValue(settings->ofClouds);
        settings->saveOptions();
        syncCheckboxes();
        return;
    case BUTTON_FOG:
        settings->ofFogOff = legacyFogToggledOff(settings->ofFogOff);
        settings->saveOptions();
        syncCheckboxes();
        return;
    case BUTTON_DEFLICKER:
        settings->wiiDeflicker = !settings->wiiDeflicker;
        PlatformUserSettings::setDisplayDeflicker(settings->wiiDeflicker);
        settings->saveOptions();
        syncCheckboxes();
        return;
    case BUTTON_FRAMERATE_LIMIT:
        settings->setOptionValue(EnumOptions::FRAMERATE_LIMIT, 1);
        button->displayString = legacyFramerateLabel(settings);
        return;
    case BUTTON_ANIMATIONS:
        settings->saveOptions();
        mc->displayGuiScreen(new GuiAnimationSettingsOF(this, settings));
        return;
    case BUTTON_QUALITY:
        settings->saveOptions();
        mc->displayGuiScreen(new GuiQualitySettingsOF(this, settings));
        return;
    case BUTTON_DETAILS:
        settings->saveOptions();
        mc->displayGuiScreen(new GuiDetailSettingsOF(this, settings));
        return;
    case BUTTON_PERFORMANCE:
        settings->saveOptions();
        mc->displayGuiScreen(new GuiPerformanceSettingsOF(this, settings));
        return;
    case BUTTON_OTHER:
        settings->saveOptions();
        mc->displayGuiScreen(new GuiOtherSettingsOF(this, settings));
        return;
    case BUTTON_DONE:
        returnToParent();
        return;
    default:
        return;
    }
}

void LegacyVideoOptions::drawScreen(int_t mouseX, int_t mouseY, float_t partialTick)
{
    drawLegacyBackground(partialTick);
    updateLegacyPointerHover(mouseX, mouseY);
    GuiScreen::drawScreen(mouseX, mouseY, partialTick);
}
