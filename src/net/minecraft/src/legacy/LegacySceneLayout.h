#pragma once

#include "java/Type.h"

struct LegacySceneLayout
{
    int_t titleY = 8;
    int_t titleMaxWidth = 0;
    int_t titleMaxHeight = 0;
    int_t contentTop = 0;
};

LegacySceneLayout legacySceneLayout(int_t screenWidth, int_t screenHeight);

// Legacy's composition was authored against a desktop window at the GUI scale the
// Legacy UI forces, which lands near 395 logical pixels tall. A console screen is
// 240, so the scene metrics scale off the real height instead of switching on a
// threshold: as fixed pixel values they pinned the logo to the top edge of a tall
// window and shrank it to half the screen width on a console.
constexpr int_t LEGACY_REFERENCE_HEIGHT = 395;

// Native size of assets/legacy/title.png. Only the ratio is used, to work out how
// much vertical room the logo really takes inside its budget.
constexpr int_t LEGACY_TITLE_ASPECT_WIDTH = 995;
constexpr int_t LEGACY_TITLE_ASPECT_HEIGHT = 205;

int_t legacyScaleToScreen(int_t screenHeight, int_t referenceValue, int_t minimum, int_t maximum);
