#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2NativeDraw.h"
#include "ps2/render/Ps2RenderState.h"

#include <gsKit.h>

GSTEXTURE* ps2_draw_resolve_texture(bool hasTexCoords);
Ps2RenderState ps2_draw_capture_render_state(bool allowCull, bool allowFog);

// Runs the optimized VU0 3D path for one array range. Returns false when the
// primitive mode or the projection is not supported, leaving the caller on the
// generic emitter. Shared by the immediate-mode path and the native mesh path.
bool ps2_try_draw_3d(unsigned int mode, int first, int count,
                     GSTEXTURE* tex, const float* mvp,
                     bool packedTerrain,
                     const Ps2NativeClampRun* clampRuns,
                     int clampRunCount,
                     const Ps2NativeSlice* slices, int sliceCount,
                     const void* vertices, int vertexStride, int vertexSize,
                     const void* texCoords, int texCoordStride, bool texCoordEnabled,
                     const void* colors, int colorStride, int colorSize,
                     bool colorEnabled, bool colorFloat,
                     bool fullyInside);

// Runtime switch (ps2_draw_3d_set_enabled) and the latest per-section frustum
// hint (ps2_draw_3d_set_fully_inside). The state supported checks in the mesh
// paths read them so they agree with ps2_try_draw_3d on what it will accept.
bool ps2_draw_3d_enabled();
bool ps2_draw_3d_fully_inside();

#endif // PS2_PLATFORM
