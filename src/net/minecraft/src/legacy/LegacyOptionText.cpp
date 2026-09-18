#include "LegacyOptionText.h"

#include "LegacyOptionStyle.h"
#include "LegacyUiTheme.h"
#include "net/minecraft/src/FontRenderer.h"

namespace
{
int_t legacyOptionEdgeColor(int_t color)
{
    const LegacyUiTheme &theme = legacyUiTheme();
    if (color == theme.selectedTextColor)
        return theme.selectedTextEdgeColor;
    if (color == theme.disabledTextColor)
        return static_cast<int_t>(0xffc8c8c8u);
    if ((color & 0x00ffffff) >= 0x00d0d0d0)
        return static_cast<int_t>(0xff202020u);
    return theme.normalTextEdgeColor;
}
}

void legacyDrawOptionText(FontRenderer *font, const std::string &text, int_t x, int_t y, int_t color)
{
    if (font == nullptr)
        return;

    // 4J's option labels read more like a tiny embossed pixel font than the
    // heavy drop shadow used by Java Gui::drawString(). Draw one restrained
    // lower-right edge, then the actual glyph at the requested colour.
    font->drawString(text, x + 1, y + 1, legacyOptionEdgeColor(color));
    font->drawString(text, x, y, color);
}

void legacyDrawCenteredOptionText(FontRenderer *font, const std::string &text, int_t centerX, int_t y, int_t color)
{
    if (font == nullptr)
        return;
    legacyDrawOptionText(font, text, centerX - font->getStringWidth(text) / 2, y, color);
}
