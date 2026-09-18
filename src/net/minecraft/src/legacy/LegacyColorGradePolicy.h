#pragma once

#include "LegacyLook.h"

#include <algorithm>
#include <cmath>

// Desktop uses the real framebuffer gamma shader. Console backends keep the
// low-cost lightmap/fog/sky approximation because they do not share the PC
// GLSL pipeline.
inline bool legacyLookGradeFramebufferPassEnabled()
{
    return true;
}

inline float legacyLookGradePreview(float color)
{
    color = std::max(0.0f, std::min(1.0f, color));
    return std::pow(color, LEGACY_LOOK_GAMMA_EXPONENT);
}
