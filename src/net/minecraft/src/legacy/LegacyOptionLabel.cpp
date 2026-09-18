#include "LegacyOptionLabel.h"

#include "net/minecraft/src/FontRenderer.h"

void legacyDrawOptionLabel(FontRenderer *font, const std::string &text, int_t x, int_t y)
{
    if (font == nullptr)
        return;
    // Plain dark glyphs, no second pass. This label sits on the light panel fill,
    // where both of the usual treatments hurt it: Java's drop shadow is a darkened
    // copy of the text colour, so dark-on-dark doubled the strokes into a smudge,
    // and the Legacy emboss puts a light edge under the glyph, which on a light
    // background washes it out instead of lifting it.
    font->drawString(text, x, y, 0x404040);
}
