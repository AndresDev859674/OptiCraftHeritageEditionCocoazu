#pragma once

#include "java/Type.h"

enum class LegacyOptionsLayoutPreset
{
    Compact,
    Wide,
    Form
};

struct LegacyOptionsLayout
{
    int_t panelX = 0;
    int_t panelY = 0;
    int_t panelWidth = 0;
    int_t panelHeight = 0;
    int_t contentX = 0;
    int_t contentWidth = 0;
    int_t firstRowY = 0;
    int_t rowHeight = 18;
    int_t rowSpacing = 3;
    int_t titleY = 8;
    int_t titleMaxWidth = 0;
    int_t titleMaxHeight = 0;

    int_t rowY(int_t index) const
    {
        return firstRowY + index * (rowHeight + rowSpacing);
    }
};

LegacyOptionsLayout legacyOptionsLayout(int_t screenWidth, int_t screenHeight, int_t rowCount,
    LegacyOptionsLayoutPreset preset = LegacyOptionsLayoutPreset::Wide);

int_t legacyOptionsPanelWidth(int_t screenWidth,
    LegacyOptionsLayoutPreset preset = LegacyOptionsLayoutPreset::Wide);

// Largest row count whose panel still starts below the title on this screen.
// Paged screens size their page from this instead of a hardcoded screen-height
// threshold, so the row metrics in LegacyUiTheme can change without leaving one
// of them overlapping the logo.
int_t legacyOptionsMaxRows(int_t screenWidth, int_t screenHeight,
    LegacyOptionsLayoutPreset preset = LegacyOptionsLayoutPreset::Wide);

int_t legacyCenteredPanelY(int_t screenWidth, int_t screenHeight, int_t panelHeight, int_t footerGap);
