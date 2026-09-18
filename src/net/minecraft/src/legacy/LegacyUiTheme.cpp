#include "LegacyUiTheme.h"

const LegacyUiTheme &legacyUiTheme()
{
    static const LegacyUiTheme theme = {
        224, // panelTargetWidth
        8,   // contentPadding
        18,  // rowHeight (Legacy4J sits its sliders and buttons around 20 px; the
             //            12 px TickBox sprite is centred in the same row)
        3,   // rowSpacing
        10,  // checkboxSize
        4,   // sliderSideInset (LegacySlider uses width - 8 effective travel)
        8,   // sliderKnobWidth
        2,   // panelCornerCut

        static_cast<int_t>(0xff606060u),
        static_cast<int_t>(0xffffff00u),
        static_cast<int_t>(0xff989898u),
        static_cast<int_t>(0xffb8b8b8u),
        static_cast<int_t>(0xff3b3b20u),

        static_cast<int_t>(0xffc6c6c6u),
        static_cast<int_t>(0xff343434u),
        static_cast<int_t>(0xffedededu),
        static_cast<int_t>(0xff858585u),
        static_cast<int_t>(0x68000000u),

        static_cast<int_t>(0xff4f4f4fu),
        static_cast<int_t>(0xff9c9c9cu),
        static_cast<int_t>(0xffb0b7cbu),
        static_cast<int_t>(0xffc2c2c2u),
        static_cast<int_t>(0xffffffffu),
        static_cast<int_t>(0xffffff00u),

        12, // checkboxTextureSize (tickbox.png is 12x12, drawn 1:1)
        16, // checkboxTickWidth  (tick.png is 28x24, kept at its 7:6 aspect)
        14, // checkboxTickHeight
        -1, // checkboxTickOffsetX
        -3, // checkboxTickOffsetY

        static_cast<int_t>(0xff222222u),
        static_cast<int_t>(0xff505050u),
        static_cast<int_t>(0xff303030u),
        static_cast<int_t>(0xff909090u),
        static_cast<int_t>(0xffb8b8b8u),
        static_cast<int_t>(0xffcbd3f2u),
        static_cast<int_t>(0xffaab7e8u)
    };
    return theme;
}
