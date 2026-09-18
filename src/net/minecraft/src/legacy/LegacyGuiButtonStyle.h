#pragma once

#include "java/Type.h"


struct LegacyGuiButtonRect
{
    int_t left;
    int_t top;
    int_t right;
    int_t bottom;
};

struct LegacyGuiButtonBorderRects
{
    LegacyGuiButtonRect top;
    LegacyGuiButtonRect bottom;
    LegacyGuiButtonRect left;
    LegacyGuiButtonRect right;
};

struct LegacyGuiButtonVisual
{
    int_t vanillaState;
    int_t textColor;
    int_t textShadowColor;
    int_t outerBorderColor;
    int_t buttonShadowColor;
    int_t hoverFillTop;
    int_t hoverFillBottom;
    int_t hoverHighlightColor;
    int_t hoverShadowColor;
    bool hovered;
    bool enabled;
};

LegacyGuiButtonVisual legacyGuiButtonVisual(bool enabled, bool hovered);
LegacyGuiButtonBorderRects legacyGuiButtonBorderRects(int_t x, int_t y, int_t width, int_t height);

int_t legacyGuiButtonTextY(int_t buttonY, int_t buttonHeight);

float_t legacyGuiButtonClampOpacity(float_t opacity);
