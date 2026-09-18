#pragma once

#ifdef PS2_PLATFORM

#include "ps2/boot/Ps2BootRenderer.h"

namespace Ps2BootText
{

float textWidth(const char* text, float pixelScale);
void draw(float x, float y, int z,
          const char* text, float pixelScale,
          Ps2BootRenderer::Color color);
void drawCentered(float centerX, float y, int z,
                  const char* text, float pixelScale,
                  Ps2BootRenderer::Color color);

} // namespace Ps2BootText

#endif // PS2_PLATFORM
