#include "TileEntitySpecialRenderer.h"
#include "TileEntityRenderer.h"
#include "FontRenderer.h"
#include "RenderEngine.h"

TileEntitySpecialRenderer::TileEntitySpecialRenderer() {
    tileEntityRenderer = nullptr;
}

void TileEntitySpecialRenderer::setTileEntityRenderer(TileEntityRenderer* renderer) {
    tileEntityRenderer = renderer;
}

FontRenderer* TileEntitySpecialRenderer::getFontRenderer() {
    if (tileEntityRenderer) {
        return tileEntityRenderer->fontRenderer;
    }
    return nullptr;
}

void TileEntitySpecialRenderer::bindTextureByName(const std::string& path) {
    RenderEngine* renderengine = nullptr;

    if (tileEntityRenderer && tileEntityRenderer->renderEngine) {
        renderengine = tileEntityRenderer->renderEngine;
    }
    else if (TileEntityRenderer::instance.renderEngine) {
        renderengine = TileEntityRenderer::instance.renderEngine;
    }

    if (renderengine) {
        int textureId = renderengine->getTexture(path);
        if (textureId >= 0) {
            renderengine->bindTexture(textureId);
        }
    }
}
