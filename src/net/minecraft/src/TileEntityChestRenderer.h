#pragma once

#include "TileEntitySpecialRenderer.h"

class ModelChest;
class TileEntityChest;

// net.minecraft.src.TileEntityChestRenderer
class TileEntityChestRenderer : public TileEntitySpecialRenderer
{
public:
	TileEntityChestRenderer();
	~TileEntityChestRenderer() override;

	void renderTileEntityChestAt(TileEntityChest *chest, double x, double y, double z, float partialTick);
	bool renderStaticPart(TileEntityChest *chest, double x, double y, double z);
	bool renderDynamicPart(TileEntityChest *chest, double x, double y, double z, float partialTick);
	void prepareStaticPart();
	void renderTileEntityAt(TileEntity *tileentity, double x, double y, double z, float partialTick) override;

private:
	bool renderTileEntityChestParts(TileEntityChest *chest, double x, double y, double z, float partialTick, bool renderStaticPart, bool renderDynamicPart);
	ModelChest *chestModel;
	ModelChest *largeChestModel;
};
