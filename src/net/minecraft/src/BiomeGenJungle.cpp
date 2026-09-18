#include "BiomeGenJungle.h"

#include "BiomeDecorator.h"
#include "Block.h"
#include "BlockTallGrass.h"
#include "EntityChicken.h"
#include "EntityOcelot.h"
#include "WorldGenBigTree.h"
#include "WorldGenHugeTrees.h"
#include "WorldGenShrub.h"
#include "WorldGenTallGrass.h"
#include "WorldGenTrees.h"
#include "WorldGenVines.h"
#include "platform/PlatformTuning.h"

#include <typeindex>

BiomeGenJungle::BiomeGenJungle()
    : jungleShrubGen(new WorldGenShrub(3, 0)),
      jungleHugeTreeGen(new WorldGenHugeTrees(false, 10, 3, 3)),
      jungleTreeGen(new WorldGenTrees(false, 4, 3, 3, true)),
      jungleFernGen(nullptr)
{
    biomeDecorator->treesPerChunk = 50;
    biomeDecorator->grassPerChunk = 25;
    biomeDecorator->flowersPerChunk = 4;

    spawnableMonsterList.push_back(SpawnListEntry(std::type_index(typeid(EntityOcelot)), 2, 1, 1));
    spawnableCreatureList.push_back(SpawnListEntry(std::type_index(typeid(EntityChicken)), 10, 4, 4));
}

BiomeGenJungle::~BiomeGenJungle()
{
    delete jungleShrubGen;
    delete jungleHugeTreeGen;
    delete jungleTreeGen;
    delete jungleFernGen;
    jungleShrubGen = nullptr;
    jungleHugeTreeGen = nullptr;
    jungleTreeGen = nullptr;
    jungleFernGen = nullptr;
}

WorldGenerator *BiomeGenJungle::getRandomWorldGenForTrees(Random &random)
{
    if (random.nextInt(10) == 0)
        return worldGenBigTree;
    if (random.nextInt(2) == 0)
        return jungleShrubGen;
    if (random.nextInt(3) == 0 && PLATFORM_POPULATE_JUNGLE_HUGE_TREES)
    {
        jungleHugeTreeGen->setBaseHeight(10 + random.nextInt(20));
        return jungleHugeTreeGen;
    }
    jungleTreeGen->setBaseHeight(4 + random.nextInt(7));
    return jungleTreeGen;
}

WorldGenerator *BiomeGenJungle::func_48410_b(Random &random)
{
    if (random.nextInt(4) == 0)
    {
        if (jungleFernGen == nullptr)
            jungleFernGen = new WorldGenTallGrass(Block::tallGrass->blockID, 2);
        return jungleFernGen;
    }
    return BiomeGenBase::func_48410_b(random);
}

bool BiomeGenJungle::isReusableWorldGenForTrees(const WorldGenerator *generator) const
{
    return generator == jungleShrubGen || generator == jungleHugeTreeGen ||
           generator == jungleTreeGen || BiomeGenBase::isReusableWorldGenForTrees(generator);
}

void BiomeGenJungle::decorate(World *world, Random &random, int_t chunkX, int_t chunkZ)
{
    BiomeGenBase::decorate(world, random, chunkX, chunkZ);

    int_t index = 0;
    while (!advanceDecorationExtra(world, random, chunkX, chunkZ, index))
    {
    }
}

bool BiomeGenJungle::advanceDecorationExtra(World *world, Random &random, int_t chunkX, int_t chunkZ, int_t &index)
{
    if (index >= PLATFORM_POPULATE_JUNGLE_VINES)
        return true;

    const int_t x = chunkX + random.nextInt(16) + 8;
    const int_t z = chunkZ + random.nextInt(16) + 8;
    WorldGenVines().generate(world, random, x, 64, z);
    ++index;
    return false;
}
