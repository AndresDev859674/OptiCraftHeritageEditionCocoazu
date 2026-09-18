#pragma once

#include "LegacyUiTheme.h"
#include "java/Type.h"

inline int_t legacyOptionCheckboxSize() { return legacyUiTheme().checkboxSize; }
inline int_t legacyOptionSliderKnobWidth() { return legacyUiTheme().sliderKnobWidth; }
inline int_t legacyOptionSliderSideInset() { return legacyUiTheme().sliderSideInset; }
inline int_t legacyOptionContentPadding() { return legacyUiTheme().contentPadding; }
inline int_t legacyOptionRowHeight() { return legacyUiTheme().rowHeight; }
inline int_t legacyOptionRowSpacing() { return legacyUiTheme().rowSpacing; }

inline int_t legacyOptionTextY(int_t rowY, int_t rowHeight)
{
    return rowY + (rowHeight - 8 + 1) / 2;
}
