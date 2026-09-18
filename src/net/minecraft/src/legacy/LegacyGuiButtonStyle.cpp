#include "LegacyGuiButtonStyle.h"

LegacyGuiButtonVisual legacyGuiButtonVisual(bool enabled, bool hovered)
{
    LegacyGuiButtonVisual visual{};
    visual.enabled = enabled;
    visual.hovered = enabled && hovered;
    visual.vanillaState = enabled ? 1 : 0;
    visual.textShadowColor = static_cast<int_t>(0x60000000u);
    // The /gui/gui.png button sprite already carries an opaque black 1px edge on
    // all four sides; an outer ring on top of it only doubles the stroke. Kept at
    // 0 so LegacyGuiButton skips it, leaving legacyGuiButtonBorderRects() available
    // for states that do want one.
    visual.outerBorderColor = 0;
    visual.buttonShadowColor = enabled
        ? static_cast<int_t>(0x78000000u)
        : static_cast<int_t>(0x50000000u);

    if (!enabled)
    {
        visual.textColor = static_cast<int_t>(0xffa0a0a0u);
        visual.hoverFillTop = 0;
        visual.hoverFillBottom = 0;
        visual.hoverHighlightColor = 0;
        visual.hoverShadowColor = 0;
        return visual;
    }

    visual.textColor = visual.hovered
        ? static_cast<int_t>(0xffffff00u)
        : static_cast<int_t>(0xfff0f0f0u);
    visual.hoverFillTop = visual.hovered
        ? static_cast<int_t>(0x72aeb9eeu)
        : 0;
    visual.hoverFillBottom = visual.hovered
        ? static_cast<int_t>(0x726f7fc7u)
        : 0;
    visual.hoverHighlightColor = visual.hovered
        ? static_cast<int_t>(0x70eef2ffu)
        : 0;
    visual.hoverShadowColor = visual.hovered
        ? static_cast<int_t>(0x80404a7eu)
        : 0;
    return visual;
}

LegacyGuiButtonBorderRects legacyGuiButtonBorderRects(int_t x, int_t y, int_t width, int_t height)
{
    LegacyGuiButtonBorderRects rects{};
    rects.top = {x - 1, y - 1, x + width + 1, y};
    rects.bottom = {x - 1, y + height, x + width + 1, y + height + 1};
    rects.left = {x - 1, y, x, y + height};
    rects.right = {x + width, y, x + width + 1, y + height};
    return rects;
}

int_t legacyGuiButtonTextY(int_t buttonY, int_t buttonHeight)
{
    return buttonY + (buttonHeight - 8 + 1) / 2;
}

float_t legacyGuiButtonClampOpacity(float_t opacity)
{
    if (opacity < 0.0f) return 0.0f;
    if (opacity > 1.0f) return 1.0f;
    return opacity;
}
