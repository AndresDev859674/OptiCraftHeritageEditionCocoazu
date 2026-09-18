#pragma once

#include "BiomeGenBase.h"

class WorldGenHugeTrees;
class WorldGenShrub;
class WorldGenTallGrass;
class WorldGenTrees;

// net.minecraft.src.BiomeGenJungle
class BiomeGenJungle : public BiomeGenBase
{
public:
    BiomeGenJungle();
    ~BiomeGenJungle() override;

    WorldGenerator *getRandomWorldGenForTrees(Random &random) override;
    WorldGenerator *func_48410_b(Random &random) override;
    void decorate(World *world, Random &random, int_t chunkX, int_t chunkZ) override;
    bool advanceDecorationExtra(World *world, Random &random, int_t chunkX, int_t chunkZ, int_t &index) override;

protected:
    bool isReusableWorldGenForTrees(const WorldGenerator *generator) const override;

private:
    WorldGenShrub *jungleShrubGen;
    WorldGenHugeTrees *jungleHugeTreeGen;
    WorldGenTrees *jungleTreeGen;
    WorldGenTallGrass *jungleFernGen;
};
