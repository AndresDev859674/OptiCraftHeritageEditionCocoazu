#pragma once

#include "BiomeGenBase.h"

class EntityWolf;
class WorldGenTaiga1;
class WorldGenTaiga2;

// net.minecraft.src.BiomeGenTaiga
class BiomeGenTaiga : public BiomeGenBase
{
public:
	BiomeGenTaiga();
	~BiomeGenTaiga() override;
	WorldGenerator *getRandomWorldGenForTrees(Random &random) override;

protected:
	bool isReusableWorldGenForTrees(const WorldGenerator *generator) const override;

private:
	WorldGenTaiga1 *taigaTreeGen1;
	WorldGenTaiga2 *taigaTreeGen2;
};
