#ifdef PS2_PLATFORM

#include "ps2/render/Ps2DrawCommon.h"

#include "ps2/render/Ps2Draw3D.h"
#include "ps2/render/Ps2ProjectionState.h"
#include "ps2/render/Ps2RenderContext.h"
#include "ps2/render/Ps2RenderGsState.h"
#include "ps2/render/Ps2RenderLighting.h"
#include "ps2/render/Ps2RenderStats.h"
#include "ps2/render/Ps2RenderTextureState.h"
#include "ps2/render/Ps2RenderTransform.h"
#include "ps2/render/Ps2RenderTypes.h"
#include "ps2/render/Ps2TextureGs.h"
#include "ps2/render/Ps2Viewport.h"

namespace
{
constexpr unsigned int kPrimTriangles = 0x0004;
constexpr unsigned int kPrimQuads = 0x0007;
constexpr unsigned int kFaceCCW = 0x0901;
constexpr unsigned int kFaceBack = ps2RenderValue(Ps2RenderFace::Back);
}

typedef float Mat4[16];

extern GSGLOBAL* gsGlobal;

// The optimized VU0 path is the shipping PS2 3D backend. The generic emitter
// remains as a runtime fallback for unsupported primitive/state combinations.
static bool s_draw_3d_enabled = true;
static bool s_draw_3d_fully_inside = false;

extern "C" void ps2_draw_3d_set_enabled(int enabled) {
	s_draw_3d_enabled = enabled != 0;
	if (!s_draw_3d_enabled)
		s_draw_3d_fully_inside = false;
}

// Per-section hint from WorldRenderer::renderPassCached: this chunk section's
// bounding box is entirely inside all six frustum planes this frame, so the
// optimized path may skip per-vertex clip outcodes for it.
extern "C" void ps2_draw_3d_set_fully_inside(int fullyInside) {
	s_draw_3d_fully_inside = fullyInside != 0;
}

bool ps2_draw_3d_enabled() { return s_draw_3d_enabled; }
bool ps2_draw_3d_fully_inside() { return s_draw_3d_fully_inside; }

GSTEXTURE* ps2_draw_resolve_texture(bool hasTexCoords)
{
    const Ps2RenderContext& context = ps2_render_context();
    if (!(context.tex2d && hasTexCoords))
        return nullptr;
    return ps2_texture_resolve(ps2_texture_bound_name());
}

Ps2RenderState ps2_draw_capture_render_state(bool allowCull, bool allowFog)
{
    const Ps2RenderContext& context = ps2_render_context();
    Ps2RenderState render;
    render.cullFace = allowCull && context.cullFace;
    render.frontFaceCCW = context.frontFace == kFaceCCW;
    render.cullBackFace = context.cullMode == kFaceBack;
    render.flatR = context.cr;
    render.flatG = context.cg;
    render.flatB = context.cb;
    render.flatA = context.ca;
    render.fogEnabled = allowFog && context.fog;
    render.fogMode = context.fogMode;
    render.fogDensity = context.fogDensity;
    render.fogStart = context.fogStart;
    render.fogEnd = context.fogEnd;
    render.fogR = context.fogR;
    render.fogG = context.fogG;
    render.fogB = context.fogB;
    render.fogLinearValid = context.fogEnd > context.fogStart;
    render.fogLinearScale = render.fogLinearValid ? 1.0f / (context.fogEnd - context.fogStart) : 0.0f;
    render.fogLinearBias = context.fogEnd * render.fogLinearScale;
    render.smoothShading = ps2_gs_state_smooth_shading();
    ps2_lighting_prepare_render_state(render);
    return render;
}

bool ps2_try_draw_3d(unsigned int mode, int first, int count,
                     GSTEXTURE* tex, const Mat4 mvp,
                     bool packedTerrain,
                     const Ps2NativeClampRun* clampRuns,
                     int clampRunCount,
                     const Ps2NativeSlice* slices, int sliceCount,
                     const void* vertices, int vertexStride, int vertexSize,
                     const void* texCoords, int texCoordStride, bool texCoordEnabled,
                     const void* colors, int colorStride, int colorSize,
                     bool colorEnabled, bool colorFloat,
                     bool fullyInside) {
	if (!s_draw_3d_enabled || (mode != kPrimTriangles && mode != kPrimQuads)
            || ps2_projection_is_orthographic())
        return false;

	Ps2Draw3DState fastState;
	fastState.packedTerrain = packedTerrain;
    fastState.clampRuns = clampRuns;
    fastState.clampRunCount = clampRunCount;
    fastState.slices = slices;
    fastState.sliceCount = sliceCount;
    fastState.tileAtlas = ps2_texture_bound_is_tile_atlas();
    fastState.quads = (mode == kPrimQuads);
    fastState.gsGlobal = gsGlobal;
    fastState.texture = tex;
    fastState.mvp = mvp;
    fastState.vertices = vertices;
    fastState.vertexStride = vertexStride;
    fastState.vertexSize = vertexSize;
    fastState.texCoords = texCoords;
    fastState.texCoordStride = texCoordStride;
    fastState.texCoordEnabled = texCoordEnabled;
    fastState.colors = colors;
    fastState.colorStride = colorStride;
    fastState.colorSize = colorSize;
    fastState.colorEnabled = colorEnabled;
    fastState.colorFloat = colorFloat;
    fastState.render = ps2_draw_capture_render_state(true, true);
    fastState.first = first;
    fastState.count = count;
    fastState.ortho = false;
    fastState.fullyInside = fullyInside;
    fastState.depth = ps2_transform_depth_map();
    fastState.depth.qBias += ps2_gs_state_polygon_depth_bias();
    fastState.forceNearZ = ps2_transform_force_near_z_enabled();
    fastState.viewW = ps2_viewport_width();
    fastState.viewH = ps2_viewport_height();
#ifdef PS2_RENDER_STATS
    fastState.debugPrims = &ps2_render_stats().draw3dPrims;
    fastState.debugClipped = &ps2_render_stats().draw3dClipped;
    fastState.debugOffscreen = &ps2_render_stats().draw3dOffscreen;
    fastState.debugBackface = &ps2_render_stats().draw3dBackface;
#else
    fastState.debugPrims = nullptr;
    fastState.debugClipped = nullptr;
    fastState.debugOffscreen = nullptr;
    fastState.debugBackface = nullptr;
#endif
    fastState.setTextureClampForUv = ps2_texture_state_set_clamp_for_uv;
    fastState.applyClampSel = ps2_texture_state_apply_clamp_sel;
	return ps2_draw_3d(fastState);
}

#endif // PS2_PLATFORM
