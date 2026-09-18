#pragma once

#ifdef PS2_PLATFORM

#include <gsKit.h>

#include "ps2/render/Ps2Texture.h"

// The gsKit-typed half of the texture module, for the GS draw paths only.
// Ps2Texture.h stays free of gsKit because platform/RenderAPI.h includes it and
// therefore so does every game translation unit.

// Returns the GSTEXTURE to draw with, flushing any deferred sub-image upload
// first. Returns nullptr when the name has no usable texture.
GSTEXTURE* ps2_texture_resolve(unsigned int name);

// TEX1 (and MIPTBP1 when the texture holds a mip chain) for a native packet
// that writes the sampler itself instead of going through a gsKit primitive.
// Always write tex1: a packet that leaves it alone inherits the previous
// draw's chain -- MXL and MIPTBP1 of a different texture -- and samples it.
struct Ps2TextureSampler {
    u64  tex1;
    u64  miptbp1;   // valid only when mipmapped
    bool mipmapped;
};
Ps2TextureSampler ps2_texture_sampler_registers(const GSTEXTURE* texture);

#endif // PS2_PLATFORM
