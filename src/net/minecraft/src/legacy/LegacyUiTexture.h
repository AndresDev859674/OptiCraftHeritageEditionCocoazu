#pragma once

#include "java/Type.h"

class RenderEngine;

// Cached handle for a standalone Legacy UI texture such as /legacy/tickbox.png.
// Legacy assets are optional on every platform, so resolve() reports -1 when the
// file is absent and each call site keeps its procedural fallback; a build that
// ships without assets/legacy still draws a usable control.
class LegacyUiTexture
{
public:
    explicit LegacyUiTexture(const char *resourcePath);

    int_t resolve(RenderEngine *engine);

private:
    const char *path;
    RenderEngine *boundEngine;
    int_t texture;
    bool resourceChecked;
    bool resourceAvailable;
};

// Draws a whole texture into the given rectangle. Blending is enabled for the
// quad and disabled again afterwards, matching legacyDrawTitleTexture.
void legacyDrawUiTexture(int_t texture, int_t x, int_t y, int_t width, int_t height, float_t zLevel);

// Nine-slice draw of a whole texture, the way Legacy4J's "nine_slice" gui
// sprites scale: the texture stands for a spriteWidth x spriteHeight rectangle
// in GUI units, of which `border` units on every side are drawn 1:1 and the
// rest is stretched. Blending is handled like legacyDrawUiTexture.
void legacyDrawUiTextureNineSlice(int_t texture, int_t x, int_t y, int_t width, int_t height,
    int_t spriteWidth, int_t spriteHeight, int_t border, float_t zLevel);
