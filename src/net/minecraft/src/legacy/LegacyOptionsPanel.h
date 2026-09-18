#pragma once

#include "net/minecraft/src/Gui.h"
#include "LegacyOptionsLayout.h"

struct LegacyPanelColors
{
    int_t fill;
    int_t border;
    int_t highlight;
    int_t shadow;
    int_t dropShadow;
};

class LegacyOptionsPanel : public Gui
{
public:
    // Theme-coloured panel used by the Legacy option/play screens.
    void draw(const LegacyOptionsLayout &layout);
    // Same rounded frame in caller-supplied colours (LegacyTipHud's dark tip).
    void drawFrame(const LegacyOptionsLayout &layout, const LegacyPanelColors &colors);
};
