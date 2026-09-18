#pragma once

#include "java/Type.h"

// Shared /gui/gui.png drawing for the Legacy controls. The vanilla widget art is
// 20 px tall while Legacy rows are shorter, so every helper stretches the source
// rectangle into the destination instead of clipping it. The caller binds
// /gui/gui.png and owns the colour state.

void legacyDrawGuiAtlasRect(int_t x, int_t y, int_t width, int_t height, int_t texX, int_t texY,
    int_t sourceWidth, int_t sourceHeight, float_t zLevel);

// Vanilla button frame, mirrored around the middle the way GuiButton does so the
// 200 px source covers any width. state is the usual 0 disabled / 1 idle / 2 hover.
void legacyDrawVanillaButtonBase(int_t x, int_t y, int_t width, int_t height, int_t state,
    float_t zLevel);

// Vanilla slider knob, the two 4 px halves GuiSlider draws at (0,66) and (196,66).
void legacyDrawVanillaSliderKnob(int_t x, int_t y, int_t width, int_t height, float_t zLevel);
