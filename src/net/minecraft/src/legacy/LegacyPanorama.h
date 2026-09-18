#pragma once

#include <algorithm>
#include <cmath>

#include "java/Type.h"

class Minecraft;

struct LegacyPanoramaUv
{
    float_t u0 = 0.0f;
    float_t v0 = 0.0f;
    float_t u1 = 1.0f;
    float_t v1 = 1.0f;
};

inline const char *legacyPanoramaResourcePath()
{
    // Keep one source asset for every platform. RenderEngine downsizes this
    // texture at upload time on Wii/PS2, preserving the original aspect ratio
    // without requiring a second console-specific panorama resource.
    return "/legacy/panorama.png";
}

inline float_t legacyPanoramaLoopTicks()
{
    // Legacy Console scrolls the panorama slowly and restarts from the first frame.
    return 3600.0f;
}

inline float_t legacyPanoramaLoopOffset(float_t ticks)
{
    const float_t period = legacyPanoramaLoopTicks();
    float_t wrapped = std::fmod(ticks, period);
    if (wrapped < 0.0f)
        wrapped += period;
    return wrapped / period;
}

inline int_t legacyPanoramaBlurSampleCount()
{
    return 5;
}

inline LegacyPanoramaUv legacyPanoramaUv(int_t screenWidth, int_t screenHeight,
    int_t textureWidth, int_t textureHeight, float_t horizontalOffset)
{
    LegacyPanoramaUv uv{};
    if (screenWidth <= 0 || screenHeight <= 0 || textureWidth <= 0 || textureHeight <= 0)
        return uv;

    horizontalOffset = std::max<float_t>(0.0f, std::min<float_t>(1.0f, horizontalOffset));
    const float_t screenAspect = static_cast<float_t>(screenWidth) / static_cast<float_t>(screenHeight);
    const float_t textureAspect = static_cast<float_t>(textureWidth) / static_cast<float_t>(textureHeight);

    if (textureAspect > screenAspect)
    {
        const float_t visibleU = screenAspect / textureAspect;
        uv.u0 = (1.0f - visibleU) * horizontalOffset;
        uv.u1 = uv.u0 + visibleU;
    }
    else if (textureAspect < screenAspect)
    {
        const float_t visibleV = textureAspect / screenAspect;
        uv.v0 = (1.0f - visibleV) * 0.5f;
        uv.v1 = uv.v0 + visibleV;
    }
    return uv;
}

bool legacyDrawPanorama(Minecraft *mc, int_t screenWidth, int_t screenHeight,
    int_t panoramaTimer, float_t partialTick, float_t zLevel);
