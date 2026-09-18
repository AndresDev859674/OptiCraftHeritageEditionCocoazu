#pragma once

#include "java/Type.h"

struct LegacyUiRect
{
    int_t x;
    int_t y;
    int_t width;
    int_t height;
};

struct LegacyMainMenuLayout
{
    int_t buttonX;
    int_t firstButtonY;
    int_t buttonWidth;
    int_t buttonHeight;
    int_t buttonSpacing;
    int_t titleY;
    int_t titleMaxWidth;
    int_t titleMaxHeight;
};

int_t legacyMainMenuButtonCount(bool hideQuitButton);
LegacyMainMenuLayout legacyMainMenuLayout(int_t screenWidth, int_t screenHeight, int_t buttonCount);
LegacyUiRect legacyFitTitleRect(int_t screenWidth, int_t y, int_t maxWidth, int_t maxHeight,
    int_t textureWidth, int_t textureHeight);
