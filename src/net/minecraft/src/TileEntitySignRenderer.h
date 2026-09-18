#pragma once

#include "TileEntitySpecialRenderer.h"

class SignModel;
class TileEntitySign;

class TileEntitySignRenderer : public TileEntitySpecialRenderer {
public:
    TileEntitySignRenderer();
    ~TileEntitySignRenderer() override;

    void renderTileEntityAt(TileEntity* tileentity, double d, double d1, double d2, float f) override;
    bool renderStaticPart(TileEntitySign *tileentitysign, double d, double d1, double d2);
    bool renderDynamicPart(TileEntitySign *tileentitysign, double d, double d1, double d2);
    void prepareStaticPart();

private:
    void renderTileEntitySignAt(TileEntitySign* tileentitysign, double d, double d1, double d2, float f);
    bool renderTileEntitySignParts(TileEntitySign *tileentitysign, double d, double d1, double d2,
        bool renderStaticPart, bool renderDynamicPart);

    SignModel* signModel;
};
