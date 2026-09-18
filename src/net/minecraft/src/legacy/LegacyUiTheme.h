#pragma once

#include "java/Type.h"

// Central visual language for Heritage's Legacy UI.  The compact control
// proportions mirror the behaviour of Legacy4J's HD TickBox/slider controls,
// while all actual drawing remains native to this 1.2.5 C++ renderer.
struct LegacyUiTheme
{
    int_t panelTargetWidth;
    int_t contentPadding;
    int_t rowHeight;
    int_t rowSpacing;
    int_t checkboxSize;
    int_t sliderSideInset;
    int_t sliderKnobWidth;
    int_t panelCornerCut;

    int_t normalTextColor;
    int_t selectedTextColor;
    int_t disabledTextColor;
    int_t normalTextEdgeColor;
    int_t selectedTextEdgeColor;

    int_t panelFillColor;
    int_t panelBorderColor;
    int_t panelHighlightColor;
    int_t panelShadowColor;
    int_t panelDropShadowColor;

    int_t checkboxBorderColor;
    int_t checkboxFillColor;
    int_t checkboxHoverFillColor;
    int_t checkboxInnerColor;
    int_t checkboxTickColor;
    int_t checkboxTickHoverColor;

    // Placement of the optional /legacy/tickbox*.png and /legacy/tick.png art.
    // The box is drawn centred on the checkboxSize rect so the label keeps its
    // position whether the textures are present or not, and the tick is offset
    // from that box: Legacy4J lets the check overhang the box towards the top
    // right instead of fitting inside it.
    int_t checkboxTextureSize;
    int_t checkboxTickWidth;
    int_t checkboxTickHeight;
    int_t checkboxTickOffsetX;
    int_t checkboxTickOffsetY;

    int_t sliderBorderColor;
    int_t sliderTopColor;
    int_t sliderBottomColor;
    int_t sliderKnobColor;
    int_t sliderKnobInnerColor;
    int_t sliderKnobHoverColor;
    int_t sliderKnobHoverInnerColor;
};

const LegacyUiTheme &legacyUiTheme();
