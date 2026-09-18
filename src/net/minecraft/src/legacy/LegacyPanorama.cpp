#include "LegacyPanorama.h"

#include "net/minecraft/src/Minecraft.h"
#include "net/minecraft/src/RenderEngine.h"
#include "net/minecraft/src/Tessellator.h"
#include "platform/RenderAPI.h"

namespace
{
void drawPanoramaQuad(int_t screenWidth, int_t screenHeight, float_t zLevel,
    const LegacyPanoramaUv &uv, float_t uOffset, float_t vOffset, float_t alpha)
{
    Tessellator *tess = &Tessellator::instance;
    tess->startDrawingQuads();
    tess->setColorRGBA_F(1.0f, 1.0f, 1.0f, alpha);
    tess->addVertexWithUV(0.0, screenHeight, zLevel, uv.u0 + uOffset, uv.v1 + vOffset);
    tess->addVertexWithUV(screenWidth, screenHeight, zLevel, uv.u1 + uOffset, uv.v1 + vOffset);
    tess->addVertexWithUV(screenWidth, 0.0, zLevel, uv.u1 + uOffset, uv.v0 + vOffset);
    tess->addVertexWithUV(0.0, 0.0, zLevel, uv.u0 + uOffset, uv.v0 + vOffset);
    tess->draw();
}
}

bool legacyDrawPanorama(Minecraft *mc, int_t screenWidth, int_t screenHeight,
    int_t panoramaTimer, float_t partialTick, float_t zLevel)
{
    if (mc == nullptr || mc->renderEngine == nullptr || screenWidth <= 0 || screenHeight <= 0)
        return false;

    const char *path = legacyPanoramaResourcePath();
    const int_t texture = mc->renderEngine->getTexture(path);
    int_t textureWidth = 0;
    int_t textureHeight = 0;
    if (!mc->renderEngine->getTextureDimensions(texture, &textureWidth, &textureHeight) ||
        textureWidth <= 0 || textureHeight <= 0)
        return false;

    const float_t ticks = static_cast<float_t>(panoramaTimer) + partialTick;
    const LegacyPanoramaUv uv = legacyPanoramaUv(screenWidth, screenHeight,
        textureWidth, textureHeight, legacyPanoramaLoopOffset(ticks));

    renderBindTexture(texture);
    renderTextureParameters(true, false, true);
    renderEnable(RenderCapability::Blend);
    renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);
    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, 0.0f, 0.0f, 1.0f);

    const float_t texelU = 1.0f / static_cast<float_t>(textureWidth);
    const float_t texelV = 1.0f / static_cast<float_t>(textureHeight);
    const float_t blurAlpha = 0.18f;
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, -texelU, 0.0f, blurAlpha);
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, texelU, 0.0f, blurAlpha);
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, 0.0f, -texelV, blurAlpha);
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, 0.0f, texelV, blurAlpha);
#ifdef PS2_PLATFORM
    // The start menu can afford four extra taps, and the diagonal samples make
    // the bilinear result read as a blur instead of a one-axis softening.
    const float_t blurDiagonalAlpha = 0.10f;
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, -texelU, -texelV, blurDiagonalAlpha);
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, texelU, -texelV, blurDiagonalAlpha);
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, -texelU, texelV, blurDiagonalAlpha);
    drawPanoramaQuad(screenWidth, screenHeight, zLevel, uv, texelU, texelV, blurDiagonalAlpha);
#endif
    return true;
}
