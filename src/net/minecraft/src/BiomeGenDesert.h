#pragma once

#include "BiomeGenBase.h"

// net.minecraft.src.BiomeGenDesert
class BiomeGenDesert : public BiomeGenBase
{
public:
    BiomeGenDesert();
    void decorate(World *world, Random &random, int_t chunkX, int_t chunkZ) override;
    bool advanceDecorationExtra(World *world, Random &random, int_t chunkX, int_t chunkZ, int_t &index) override;
};
