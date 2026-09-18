#pragma once

#include "platform/RenderAPI.h"

#if PLATFORM_PC

#define PC_RENDER_BACKEND_FUNCTIONS(X) \
    X(void, renderEnable, (RenderCapability capability), (capability)) \
    X(void, renderDisable, (RenderCapability capability), (capability)) \
    X(void, renderBlendFunc, (RenderBlendFactor source, RenderBlendFactor destination), (source, destination)) \
    X(void, renderDepthMask, (bool enabled), (enabled)) \
    X(void, renderDepthFunc, (RenderCompare function), (function)) \
    X(void, renderAlphaFunc, (RenderCompare function, float reference), (function, reference)) \
    X(void, renderCullFace, (RenderFace face), (face)) \
    X(void, renderColorMask, (bool red, bool green, bool blue, bool alpha), (red, green, blue, alpha)) \
    X(void, renderBindTexture, (int texture), (texture)) \
    X(void, renderSetActiveTextureUnit, (int textureUnit), (textureUnit)) \
    X(void, renderSetClientActiveTextureUnit, (int textureUnit), (textureUnit)) \
    X(void, renderSetMultiTextureCoord, (int textureUnit, float u, float v), (textureUnit, u, v)) \
    X(void, renderSetLightmapColors, (const std::uint32_t* colors, int count), (colors, count)) \
    X(void, renderColor4f, (float r, float g, float b, float a), (r, g, b, a)) \
    X(void, renderColor3f, (float r, float g, float b), (r, g, b)) \
    X(void, renderNormal3f, (float x, float y, float z), (x, y, z)) \
    X(void, renderGenerateTextures, (int count, int* textures), (count, textures)) \
    X(void, renderDeleteTextures, (int count, const int* textures), (count, textures)) \
    X(void, renderTextureSubImageRgba, (int level, int x, int y, int width, int height, const void* pixels), (level, x, y, width, height, pixels)) \
    X(void, renderTextureImageRgba, (int level, int width, int height, const void* pixels), (level, width, height, pixels)) \
    X(void, renderTextureParameters, (bool blur, bool mipmaps, bool clamp), (blur, mipmaps, clamp)) \
    X(void, renderApplyTextureQuality, (bool blur, int mipmapLevel, bool mipmapLinear, int anisotropy), (blur, mipmapLevel, mipmapLinear, anisotropy)) \
    X(int, renderGetMaxAnisotropy, (), ()) \
    X(int, renderGetMaxSamples, (), ()) \
    X(bool, renderTextureBeginUpload, (int texture, int width, int height, int maxLevel, bool blur, bool clamp, bool tileAtlas, bool highPrecision), (texture, width, height, maxLevel, blur, clamp, tileAtlas, highPrecision)) \
    X(bool, renderTextureIsValid, (int texture), (texture)) \
    X(void, renderResetResources, (), ()) \
    X(void, renderFogf, (RenderFogParameter parameter, float value), (parameter, value)) \
    X(void, renderFogi, (RenderFogParameter parameter, RenderFogMode value), (parameter, value)) \
    X(void, renderFogColor, (const float* values), (values)) \
    X(void, renderLightfv, (int lightIndex, RenderLightParameter parameter, const float* values), (lightIndex, parameter, values)) \
    X(void, renderLightModelAmbient, (const float* values), (values)) \
    X(void, renderColorMaterial, (RenderFace face, RenderColorMaterialMode mode), (face, mode)) \
    X(void, renderShadeModel, (RenderShadeModel model), (model)) \
    X(void, renderClear, (unsigned int mask), (mask)) \
    X(void, renderFinishGpu, (), ()) \
    X(void, renderSubmitFrame, (), ()) \
    X(void, renderClearColor, (float r, float g, float b, float a), (r, g, b, a)) \
    X(void, renderClearDepth, (double depth), (depth)) \
    X(void, renderPolygonOffset, (float factor, float units), (factor, units)) \
    X(void, renderLineWidth, (float width), (width)) \
    X(void, renderViewport, (int x, int y, int width, int height), (x, y, width, height)) \
    X(void, renderGetViewport, (int* values), (values)) \
    X(void, renderGetMatrix, (RenderMatrixQuery query, float* values), (query, values)) \
    X(const unsigned char*, renderGetString, (RenderStringQuery query), (query)) \
    X(bool, renderSupportsFeature, (RenderFeature feature), (feature)) \
    X(unsigned int, renderGetError, (), ()) \
    X(void, renderFogHint, (RenderHintMode mode), (mode)) \
    X(void, renderMatrixMode, (RenderMatrixMode mode), (mode)) \
    X(void, renderLoadIdentity, (), ()) \
    X(void, renderPushMatrix, (), ()) \
    X(void, renderPopMatrix, (), ()) \
    X(void, renderTranslate, (float x, float y, float z), (x, y, z)) \
    X(void, renderRotate, (float angle, float x, float y, float z), (angle, x, y, z)) \
    X(void, renderScale, (float x, float y, float z), (x, y, z)) \
    X(void, renderScaleDouble, (double x, double y, double z), (x, y, z)) \
    X(void, renderFrustum, (double left, double right, double bottom, double top, double nearValue, double farValue), (left, right, bottom, top, nearValue, farValue)) \
    X(void, renderOrtho, (double left, double right, double bottom, double top, double nearValue, double farValue), (left, right, bottom, top, nearValue, farValue)) \
    X(int, renderGenerateDisplayLists, (int count), (count)) \
    X(void, renderDeleteDisplayLists, (int first, int count), (first, count)) \
    X(void, renderBeginDisplayList, (int list), (list)) \
    X(void, renderEndDisplayList, (), ()) \
    X(void, renderCallDisplayList, (int list), (list)) \
    X(void, renderCallDisplayLists, (int count, const int* lists), (count, lists)) \
    X(void, renderGenerateOcclusionQueries, (int count, int* queries), (count, queries)) \
    X(void, renderBeginOcclusionQuery, (int query), (query)) \
    X(void, renderEndOcclusionQuery, (), ()) \
    X(bool, renderOcclusionQueryResultAvailable, (int query), (query)) \
    X(unsigned int, renderOcclusionQueryResult, (int query), (query)) \
    X(bool, renderReadPixelsRgb, (int x, int y, int width, int height, void* pixels), (x, y, width, height, pixels)) \
    X(bool, renderCopyFramebufferToBoundTexture, (int x, int y, int width, int height), (x, y, width, height)) \
    X(void, renderSetLegacyPresentationGamma, (bool enabled), (enabled)) \
    X(bool, renderDrawInterleaved, (const RenderInterleavedMesh& mesh), (mesh)) \
    X(bool, renderCaptureInterleaved, (const RenderInterleavedMesh& mesh, RenderCapturedMesh& out, bool append), (mesh, out, append)) \
    X(bool, renderDrawCaptured, (const RenderCapturedMesh& mesh), (mesh))

namespace PcOpenGlRenderBackend
{
#define PC_RENDER_DECLARE(returnType, name, signature, callArgs) returnType name signature;
PC_RENDER_BACKEND_FUNCTIONS(PC_RENDER_DECLARE)
#undef PC_RENDER_DECLARE
}

namespace PcD3D9RenderBackend
{
#define PC_RENDER_DECLARE(returnType, name, signature, callArgs) returnType name signature;
PC_RENDER_BACKEND_FUNCTIONS(PC_RENDER_DECLARE)
#undef PC_RENDER_DECLARE
}

#endif
