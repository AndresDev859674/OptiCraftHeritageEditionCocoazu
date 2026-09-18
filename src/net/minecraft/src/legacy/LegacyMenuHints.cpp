#include "LegacyMenuHints.h"

#include <string>

#include "net/minecraft/src/FontRenderer.h"
#include "platform/PlatformConfig.h"

void drawLegacyMenuHints(FontRenderer *font, int_t, int_t screenHeight, bool showBack)
{
    if (font == nullptr)
        return;

#if PLATFORM_PS2
    const std::string navigate = "[D-Pad] Navigate";
    const std::string select = "[X] Select";
    const std::string back = "[O] Back";
#elif PLATFORM_WII
    const std::string navigate = "[D-Pad] Navigate";
    const std::string select = "[A] Select";
    const std::string back = "[B] Back";
#else
    const std::string navigate = "[Up/Down] Navigate";
    const std::string select = "[Enter] Select";
    const std::string back = "[Esc] Back";
#endif

    const int_t y = legacyHintRowY(screenHeight);
    int_t x = LEGACY_HINT_MARGIN;
    font->drawStringWithShadow(navigate, x, y, 0xf0f0f0);
    x += font->getStringWidth(navigate) + LEGACY_HINT_GAP;
    font->drawStringWithShadow(select, x, y, 0xf0f0f0);
    if (showBack)
    {
        x += font->getStringWidth(select) + LEGACY_HINT_GAP;
        font->drawStringWithShadow(back, x, y, 0xf0f0f0);
    }
}
