#ifdef PS2_PLATFORM

#include "platform/Log.h"
#include <gsKit.h>
#include <dmaKit.h>
#include <gsPrimitive.h>
#include <gsTexture.h>
#include <gsMisc.h>
#include <gsCore.h>
#include <gsInline.h>
#include <libvux.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <malloc.h>
#include <cstdio>

// Unconditional: the generic native array path uses ClipVert, the outcodes and
// the clamp selector from this header whether or not the fast path is built.
#include "ps2/render/Ps2ClipGuard.h"
#include "ps2/render/Ps2MatrixStack.h"
#include "ps2/render/Ps2NativeDraw.h"
#include "ps2/render/Ps2Draw2D.h"
#include "ps2/render/Ps2RenderState.h"
#include "ps2/render/Ps2RenderStats.h"
#include "ps2/render/Ps2TextureGs.h"
#include "ps2/render/Ps2RenderApi.h"
#include "ps2/render/Ps2TerrainRenderer.h"
#ifdef PS2_ENABLE_VU1_TERRAIN
#include "ps2/render/Ps2Vu1Terrain.h"
#endif
#include "ps2/render/Ps2Draw3D.h"

#include "ps2/render/Ps2RenderBackend.h"
#include "ps2/render/Ps2RenderContext.h"
#include "ps2/render/Ps2RenderFrame.h"
#include "ps2/render/Ps2RenderGsState.h"
#include "ps2/render/Ps2RenderLighting.h"
#include "ps2/render/Ps2RenderTransform.h"
#include "ps2/render/Ps2RenderTextureState.h"
#include "ps2/render/Ps2GsQueue.h"
#include "ps2/render/Ps2RenderTypes.h"
#include "ps2/render/Ps2Viewport.h"
#include "ps2/render/Ps2ProjectionState.h"


namespace {
constexpr unsigned int kPrimPoints = 0x0000;
constexpr unsigned int kPrimLines = 0x0001;
constexpr unsigned int kPrimLineLoop = 0x0002;
constexpr unsigned int kPrimLineStrip = 0x0003;
constexpr unsigned int kPrimTriangles = 0x0004;
constexpr unsigned int kPrimTriangleStrip = 0x0005;
constexpr unsigned int kPrimTriangleFan = 0x0006;
constexpr unsigned int kPrimQuads = 0x0007;
constexpr unsigned int kTypeByte = 0x1400;
constexpr unsigned int kTypeFloat = 0x1406;
constexpr unsigned int kCompareNever = ps2RenderValue(Ps2RenderCompare::Never);
constexpr unsigned int kCompareLess = ps2RenderValue(Ps2RenderCompare::Less);
constexpr unsigned int kCompareEqual = ps2RenderValue(Ps2RenderCompare::Equal);
constexpr unsigned int kCompareLessEqual = ps2RenderValue(Ps2RenderCompare::LessEqual);
constexpr unsigned int kCompareGreater = ps2RenderValue(Ps2RenderCompare::Greater);
constexpr unsigned int kCompareNotEqual = ps2RenderValue(Ps2RenderCompare::NotEqual);
constexpr unsigned int kCompareGreaterEqual = ps2RenderValue(Ps2RenderCompare::GreaterEqual);
constexpr unsigned int kCompareAlways = ps2RenderValue(Ps2RenderCompare::Always);
constexpr unsigned int kBlendZero = ps2RenderValue(Ps2RenderBlendFactor::Zero);
constexpr unsigned int kBlendOne = ps2RenderValue(Ps2RenderBlendFactor::One);
constexpr unsigned int kBlendSrcColor = ps2RenderValue(Ps2RenderBlendFactor::SrcColor);
constexpr unsigned int kBlendOneMinusSrcColor = ps2RenderValue(Ps2RenderBlendFactor::OneMinusSrcColor);
constexpr unsigned int kBlendSrcAlpha = ps2RenderValue(Ps2RenderBlendFactor::SrcAlpha);
constexpr unsigned int kBlendOneMinusSrcAlpha = ps2RenderValue(Ps2RenderBlendFactor::OneMinusSrcAlpha);
constexpr unsigned int kBlendDstColor = ps2RenderValue(Ps2RenderBlendFactor::DstColor);
constexpr unsigned int kBlendOneMinusDstColor = ps2RenderValue(Ps2RenderBlendFactor::OneMinusDstColor);
constexpr unsigned int kBlendEquationAdd = 0x8006;
constexpr unsigned int kBlendEquationSubtract = 0x800A;
constexpr unsigned int kBlendEquationReverseSubtract = 0x800B;
constexpr unsigned int kCapFog = ps2RenderValue(Ps2RenderCapability::Fog);
constexpr unsigned int kCapLighting = ps2RenderValue(Ps2RenderCapability::Lighting);
constexpr unsigned int kCapDepthTest = ps2RenderValue(Ps2RenderCapability::DepthTest);
constexpr unsigned int kCapAlphaTest = ps2RenderValue(Ps2RenderCapability::AlphaTest);
constexpr unsigned int kCapBlend = ps2RenderValue(Ps2RenderCapability::Blend);
constexpr unsigned int kCapCullFace = ps2RenderValue(Ps2RenderCapability::CullFace);
constexpr unsigned int kCapTexture2D = ps2RenderValue(Ps2RenderCapability::Texture2D);
constexpr unsigned int kCapNormalize = ps2RenderValue(Ps2RenderCapability::Normalize);
constexpr unsigned int kCapRescaleNormal = ps2RenderValue(Ps2RenderCapability::RescaleNormal);
constexpr unsigned int kCapColorMaterial = ps2RenderValue(Ps2RenderCapability::ColorMaterial);
constexpr unsigned int kCapLight0 = ps2RenderValue(Ps2RenderCapability::Light0);
constexpr unsigned int kCapLight1 = ps2RenderValue(Ps2RenderCapability::Light1);
constexpr unsigned int kCapPolygonOffsetFill = ps2RenderValue(Ps2RenderCapability::PolygonOffsetFill);
constexpr unsigned int kArrayVertex = 0x8074;
constexpr unsigned int kArrayNormal = 0x8075;
constexpr unsigned int kArrayColor = 0x8076;
constexpr unsigned int kArrayTexCoord = 0x8078;
constexpr unsigned int kFogStart = ps2RenderValue(Ps2RenderFogParameter::Start);
constexpr unsigned int kFogEnd = ps2RenderValue(Ps2RenderFogParameter::End);
constexpr unsigned int kFogMode = ps2RenderValue(Ps2RenderFogParameter::Mode);
constexpr unsigned int kFogColor = ps2RenderValue(Ps2RenderFogParameter::Color);
constexpr unsigned int kFogExp = ps2RenderValue(Ps2RenderFogMode::Exp);
constexpr unsigned int kFogExp2 = ps2RenderValue(Ps2RenderFogMode::Exp2);
constexpr unsigned int kFogLinear = ps2RenderValue(Ps2RenderFogMode::Linear);
constexpr unsigned int kFaceCW = 0x0900;
constexpr unsigned int kFaceCCW = 0x0901;
constexpr unsigned int kFaceFront = ps2RenderValue(Ps2RenderFace::Front);
constexpr unsigned int kFaceBack = ps2RenderValue(Ps2RenderFace::Back);
constexpr unsigned int kShadeFlat = ps2RenderValue(Ps2RenderShadeModel::Flat);
constexpr unsigned int kShadeSmooth = ps2RenderValue(Ps2RenderShadeModel::Smooth);
constexpr unsigned int kClearColorBit = Ps2RenderClearMask::Color;
constexpr unsigned int kClearDepthBit = Ps2RenderClearMask::Depth;
constexpr unsigned int kFilterNearestMipmapLinear = 0x2702;
constexpr unsigned int kTextureMagFilter = 0x2800;
constexpr unsigned int kTextureMinFilter = 0x2801;
constexpr unsigned int kTextureWrapS = 0x2802;
constexpr unsigned int kTextureWrapT = 0x2803;
constexpr unsigned int kTextureClamp = 0x2900;
constexpr unsigned int kMatrixModelView = ps2RenderValue(Ps2RenderMatrixMode::ModelView);
constexpr unsigned int kMatrixProjection = ps2RenderValue(Ps2RenderMatrixMode::Projection);
constexpr unsigned int kLightAmbient = ps2RenderValue(Ps2RenderLightParameter::Ambient);
constexpr unsigned int kLightDiffuse = ps2RenderValue(Ps2RenderLightParameter::Diffuse);
constexpr unsigned int kLightSpecular = ps2RenderValue(Ps2RenderLightParameter::Specular);
constexpr unsigned int kLightPosition = ps2RenderValue(Ps2RenderLightParameter::Position);
constexpr unsigned int kLightModelAmbient = 0x0B53;
constexpr unsigned int kAmbientAndDiffuse = ps2RenderValue(Ps2RenderColorMaterialMode::AmbientAndDiffuse);
} // namespace

// ---- Clear / frame ----

void ps2_render_clear_color(float r, float g, float b, float a) {
    ps2_render_frame_set_clear_color(r, g, b, a);
}

void ps2_render_clear(unsigned int mask) {
    ps2_render_frame_clear(mask);
}


// ---- Native fixed-function state ----

static Ps2RenderContext& st = ps2_render_context();

// ---- Helpers ----

static inline unsigned char clamp_colorf(float value) {
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 255;
    return static_cast<unsigned char>(value * 255.0f);
}

// ---- Native fixed-function implementation ----

// High-level fixed-function GS state is owned by Ps2RenderGsState.

void ps2_native_begin_terrain_pass(unsigned int textureId, Ps2NativeTerrainPass pass) {
    st.tex2d = true;
    ps2_texture_bind(textureId);
    ps2_gs_state_begin_terrain_pass(pass == PS2_NATIVE_TERRAIN_TRANSLUCENT);
}

void ps2_native_end_terrain_pass(Ps2NativeTerrainPass pass) {
    ps2_gs_state_end_terrain_pass(pass == PS2_NATIVE_TERRAIN_TRANSLUCENT);
}

void ps2_render_blend_func(unsigned int source, unsigned int destination) {
    ps2_gs_state_set_blend_func(source, destination);
}

extern "C" void ps2_force_fix_blend(int enable, unsigned char fix) {
    ps2_gs_state_force_fix_blend(enable != 0, fix);
}

// ---- Alpha test (GS TEST register) ----
void ps2_render_alpha_func(unsigned int compare, float reference) {
    st.alphaFunc = compare;
    if (reference <= 0.0f) st.alphaRef = 0;
    else if (reference >= 1.0f) st.alphaRef = 255;
    else st.alphaRef = (u8)(reference * 255.0f);
}

// Depth clear and depth-state translation are owned by Ps2RenderGsState.

#define PS2_kCapFog        0x0B60
#define PS2_kFogStart  0x0B63
#define PS2_kFogEnd    0x0B64
#define PS2_kFogColor  0x0B66

void ps2_render_enable(unsigned int cap) {
    if (cap == kCapTexture2D) st.tex2d = true;
    else if (cap == kCapBlend) {
        st.blend = true;
        ps2_gs_state_apply_blend();
    }
    else if (cap == kCapAlphaTest) st.alphaTest = true;
    else if (cap == kCapCullFace) st.cullFace = true;
    else if (cap == kCapDepthTest) ps2_gs_state_set_depth_test(true);
    else if (cap == PS2_kCapFog) st.fog = true;
    else if (cap == kCapLighting) ps2_lighting_set_enabled(true);
    else if (cap == kCapLight0) ps2_lighting_set_light_enabled(0, true);
    else if (cap == kCapLight1) ps2_lighting_set_light_enabled(1, true);
    else if (cap == kCapPolygonOffsetFill) ps2_gs_state_set_polygon_offset_enabled(true);
    else if (cap == kCapNormalize || cap == kCapRescaleNormal) {
        // ps2_light_vertex normalizes the eye-space normal unconditionally, so
        // NORMALIZE/RESCALE_NORMAL need no separate state.
    }
}

void ps2_render_disable(unsigned int cap) {
    if (cap == kCapTexture2D) st.tex2d = false;
    else if (cap == kCapBlend) st.blend = false;
    else if (cap == kCapAlphaTest) st.alphaTest = false;
    else if (cap == kCapCullFace) st.cullFace = false;
    else if (cap == kCapDepthTest) ps2_gs_state_set_depth_test(false);
    else if (cap == PS2_kCapFog) st.fog = false;
    else if (cap == kCapLighting) ps2_lighting_set_enabled(false);
    else if (cap == kCapLight0) ps2_lighting_set_light_enabled(0, false);
    else if (cap == kCapLight1) ps2_lighting_set_light_enabled(1, false);
    else if (cap == kCapPolygonOffsetFill) ps2_gs_state_set_polygon_offset_enabled(false);
    else if (cap == kCapNormalize || cap == kCapRescaleNormal) {
        // No-op, but accepted.
    }
}

void ps2_render_fogf(unsigned int parameter, float value) {
    if (parameter == kFogStart) st.fogStart = value;
    else if (parameter == kFogEnd) st.fogEnd = value;
    else if (parameter == ps2RenderValue(Ps2RenderFogParameter::Density)) st.fogDensity = value;
}

void ps2_render_fog_color(const float* values) {
    if (!values) return;
    st.fogR = values[0]; st.fogG = values[1]; st.fogB = values[2];
}

void ps2_render_fog_mode(unsigned int mode) {
    if (mode == kFogLinear || mode == kFogExp || mode == kFogExp2)
        st.fogMode = mode;
}


void ps2_render_cull_face(unsigned int face) {
    if (face == kFaceFront || face == kFaceBack) st.cullMode = (unsigned int)face;
}


void ps2_render_depth_func(unsigned int compare) {
    ps2_gs_state_set_depth_func(compare);
}

void ps2_render_depth_mask(bool enabled) {
    ps2_gs_state_set_depth_mask(enabled);
}

void ps2_render_color_mask(bool red, bool green, bool blue, bool alpha) {
    ps2_gs_state_set_color_mask(red, green, blue, alpha);
}

void ps2_render_shade_model(unsigned int model) {
    ps2_gs_state_set_shade_model(model);
}

void ps2_render_viewport(int x, int y, int width, int height) {
    ps2_transform_viewport(x, y, width, height);
}

void ps2_render_get_viewport(int* values) {
    ps2_transform_get_viewport(values);
}

// Directional lights only: Minecraft never uses positional lights, and the w
// component it passes is always 0. The direction is taken into eye space with
// the modelview that is current right now, which is what GL does at the moment
// glLight is called — GuiContainer relies on it (it sets the item lights inside
// a 120-degree X rotation) and so does the world entity pass (camera matrix).
void ps2_render_lightfv(int lightIndex, unsigned int parameter, const float* params) {
    ps2_lighting_lightfv(lightIndex, parameter, params);
}


void ps2_render_light_model_ambient(const float* values) {
    ps2_lighting_light_model_ambient(values);
}

// The PS2 lighting path treats the vertex colour as the material, which is
// exactly what kAmbientAndDiffuse means and the only mode Minecraft ever
// selects. There is no separate material to track, so this records nothing.
void ps2_render_color_material(unsigned int face, unsigned int mode) {
    (void)face;
    (void)mode;
}

void ps2_render_set_lightmap_enabled(bool enabled) {
    ps2_lighting_set_lightmap_enabled(enabled);
}

void ps2_render_set_lightmap_coord(float u, float v) {
    ps2_lighting_set_lightmap_coord(u, v);
}

void ps2_render_set_lightmap_colors(const std::uint32_t* colors, int count) {
    ps2_lighting_set_lightmap_colors(colors, count);
}

const std::uint32_t* ps2_render_get_lightmap_colors() {
    return ps2_lighting_get_lightmap_colors();
}


void ps2_render_color4f(float r, float g, float b, float a) {
    st.cr=clamp_colorf(r); st.cg=clamp_colorf(g);
    st.cb=clamp_colorf(b); st.ca=clamp_colorf(a);
}

// The GS interpolates the vertex normal only through the lighting path, which
// reads its normals from the client array. A single current normal has no
// consumer here, so this is accepted and dropped -- the same contract the
// glNormal3f stub had.
void ps2_render_normal3f(float x, float y, float z) {
    (void)x;
    (void)y;
    (void)z;
}

// Owns what the glFrustum stub in gles_ps2.h used to build inline. The GS depth
// mapping needs the near/far planes and they cannot be recovered from clip
// space, so they are recorded here before the matrix is multiplied in.
void ps2_render_frustum(float left, float right, float bottom, float top,
                        float nearValue, float farValue) {
    ps2_transform_frustum(left, right, bottom, top, nearValue, farValue);
}

void ps2_render_ortho(float left, float right, float bottom, float top,
                      float nearValue, float farValue) {
    ps2_transform_ortho(left, right, bottom, top, nearValue, farValue);
}


// ---- Matrix stack functions ----

void ps2_render_matrix_mode(unsigned int mode) {
    ps2_transform_matrix_mode(mode);
}

void ps2_render_load_identity() {
    ps2_transform_load_identity();
}

void ps2_render_push_matrix() {
    ps2_transform_push_matrix();
}

void ps2_render_pop_matrix() {
    ps2_transform_pop_matrix();
}

void ps2_render_get_matrix(unsigned int query, float* values) {
    ps2_transform_get_matrix(query, values);
}


void ps2_render_translate(float x, float y, float z) {
    ps2_transform_translate(x, y, z);
}
void ps2_render_scale(float x, float y, float z) {
    ps2_transform_scale(x, y, z);
}
void ps2_render_rotate(float angle, float ax, float ay, float az) {
    ps2_transform_rotate(angle, ax, ay, az);
}


// The remaining native entry points have no state of their own on this backend.
// They are real functions rather than header stubs so the reason each one does
// nothing is recorded once, next to the pipeline it would have driven.
//
//   clear_depth      ps2_clear_depth_only() writes the far value unconditionally;
//                    the GS has no clear-value register to preload.
//   polygon_offset   the GS rasterises with no depth bias unit. Minecraft uses
//                    this only for the block-breaking overlay, which the PS2
//                    path already separates with its own draw order.
//   line_width       GS lines are one pixel wide; there is no width register.
//   fog_hint         the fog blend is per-vertex linear either way.
void ps2_render_clear_depth(float depth) {
    (void)depth;
}
void ps2_render_polygon_offset(float factor, float units) {
    (void)factor;
    ps2_gs_state_set_polygon_offset_units(units);
}
void ps2_render_line_width(float width) {
    (void)width;
}
void ps2_render_fog_hint(bool nicest) {
    (void)nicest;
}

const unsigned char* ps2_render_get_string() {
    return (const unsigned char*)"PS2";
}

// The backend reports its failures through the unified PS2 log and by
// invalidating the object that failed (see ps2_texture_upload_rgba), not
// through a deferred error queue nothing reads.
unsigned int ps2_render_get_error() {
    return PS2_RENDER_NO_ERROR;
}

#endif // PS2_PLATFORM
