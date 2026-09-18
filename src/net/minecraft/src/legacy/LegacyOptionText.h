#pragma once

#include "java/Type.h"
#include <string>

class FontRenderer;

void legacyDrawOptionText(FontRenderer *font, const std::string &text, int_t x, int_t y, int_t color);
void legacyDrawCenteredOptionText(FontRenderer *font, const std::string &text, int_t centerX, int_t y, int_t color);
