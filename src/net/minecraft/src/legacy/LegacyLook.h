#pragma once

#include "java/Type.h"

// Legacy4J-style gamma curve used by OptiCraft's Legacy Look.
//
// Legacy4J sends this value to its gamma shader:
//     effectiveGamma = option * 1.5 + 0.5
// and the shader applies:
//     out = pow(in, 1.0 / effectiveGamma)
//
// OptiCraft exposes Legacy Look as a boolean, so we keep a fixed reference
// option value. 0.5 -> effective gamma 1.25 -> exponent 0.8.
// This is deliberately kept in one place so a future slider can reuse the
// exact same implementation without touching the renderer.
constexpr float_t LEGACY_LOOK_GAMMA_SETTING = 0.5f;
constexpr float_t LEGACY_LOOK_EFFECTIVE_GAMMA =
    LEGACY_LOOK_GAMMA_SETTING * 1.5f + 0.5f;
constexpr float_t LEGACY_LOOK_GAMMA_EXPONENT =
    1.0f / LEGACY_LOOK_EFFECTIVE_GAMMA;

bool legacyLookDefaultEnabled();
float_t legacyLookChannel(float_t value);
unsigned char legacyLookByte(unsigned char value);
void legacyLookRgb(float_t &r, float_t &g, float_t &b);
