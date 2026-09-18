#include "BiomeGenTaiga.h"

#include <typeindex>

#include "BiomeDecorator.h"
#include "EntityWolf.h"
#include "WorldGenTaiga1.h"
#include "WorldGenTaiga2.h"

BiomeGenTaiga::BiomeGenTaiga()
    : taigaTreeGen1(new WorldGenTaiga1()),
      taigaTreeGen2(new WorldGenTaiga2(false))
{
    spawnableCreatureList.push_back(SpawnListEntry(std::type_index(typeid(EntityWolf)), 8, 4, 4));
    biomeDecorator->treesPerChunk = 10;
    biomeDecorator->grassPerChunk = 1;
}

BiomeGenTaiga::~BiomeGenTaiga()
{
    delete taigaTreeGen1;
    delete taigaTreeGen2;
    taigaTreeGen1 = nullptr;
    taigaTreeGen2 = nullptr;
}

WorldGenerator *BiomeGenTaiga::getRandomWorldGenForTrees(Random &random)
{
    if (random.nextInt(3) == 0)
        return taigaTreeGen1;
    return taigaTreeGen2;
}

bool BiomeGenTaiga::isReusableWorldGenForTrees(const WorldGenerator *generator) const
{
    return generator == taigaTreeGen1 || generator == taigaTreeGen2 ||
           BiomeGenBase::isReusableWorldGenForTrees(generator);
}
