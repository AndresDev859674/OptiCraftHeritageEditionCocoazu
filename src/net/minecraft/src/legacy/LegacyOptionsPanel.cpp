#include "LegacyOptionsPanel.h"

#include "LegacyOptionStyle.h"
#include "LegacyUiTheme.h"

void LegacyOptionsPanel::draw(const LegacyOptionsLayout &layout)
{
    const LegacyUiTheme &theme = legacyUiTheme();
    const LegacyPanelColors colors = {
        theme.panelFillColor, theme.panelBorderColor, theme.panelHighlightColor,
        theme.panelShadowColor, theme.panelDropShadowColor
    };
    drawFrame(layout, colors);
}

void LegacyOptionsPanel::drawFrame(const LegacyOptionsLayout &layout, const LegacyPanelColors &colors)
{
    const LegacyUiTheme &theme = legacyUiTheme();
    const int_t left = layout.panelX;
    const int_t top = layout.panelY;
    const int_t right = left + layout.panelWidth;
    const int_t bottom = top + layout.panelHeight;
    const int_t cut = theme.panelCornerCut;

    const auto roundedFill = [this, cut](int_t l, int_t t, int_t r, int_t b, int_t color)
    {
        drawRect(l + cut, t, r - cut, b, color);
        drawRect(l, t + cut, r, b - cut, color);
        drawRect(l + 1, t + 1, r - 1, t + cut, color);
        drawRect(l + 1, b - cut, r - 1, b - 1, color);
    };

    roundedFill(left + 2, top + 2, right + 2, bottom + 2, colors.dropShadow);
    roundedFill(left - 1, top - 1, right + 1, bottom + 1, colors.border);
    roundedFill(left, top, right, bottom, colors.fill);

    drawRect(left + 2, top + 1, right - 2, top + 2, colors.highlight);
    drawRect(left + 1, top + 2, left + 2, bottom - 2, colors.highlight);
    drawRect(left + 2, bottom - 2, right - 2, bottom - 1, colors.shadow);
    drawRect(right - 2, top + 2, right - 1, bottom - 2, colors.shadow);
}
