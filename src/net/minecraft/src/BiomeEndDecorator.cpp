#include "BiomeEndDecorator.h"

#include "Block.h"
#include "EndDragonSpawnPolicy.h"
#include "EndDragonState.h"
#include "EntityDragon.h"
#include "World.h"
#include "WorldGenSpikes.h"

namespace
{
    bool hasLivingEnderDragon(World *world)
    {
        if (world == nullptr)
            return false;

        for (Entity *entity : world->loadedEntityList)
        {
            EntityDragon *dragon = dynamic_cast<EntityDragon *>(entity);
            if (dragon != nullptr && !dragon->isDead)
                return true;
        }
        return false;
    }
}

BiomeEndDecorator::BiomeEndDecorator(BiomeGenBase *biome)
    : BiomeDecorator(biome), spikeGen(new WorldGenSpikes(Block::whiteStone->blockID))
{
}

void BiomeEndDecorator::decorate()
{
    generateOres();

    if (randomGenerator->nextInt(5) == 0)
    {
        const int_t x = chunk_X + randomGenerator->nextInt(16) + 8;
        const int_t z = chunk_Z + randomGenerator->nextInt(16) + 8;
        const int_t y = currentWorld->getTopSolidOrLiquidBlock(x, z);
        spikeGen->generate(currentWorld, *randomGenerator, x, y, z);
    }

    // The central chunk is decorated again whenever a console regenerates it
    // (its population edits are not persisted), so the dragon is gated on the
    // saved defeat state and on a live dragon instead of being unconditional.
    if (!EndDragonSpawnPolicy::isCentralChunk(chunk_X, chunk_Z))
        return;

    const bool defeated = EndDragonState::get(currentWorld)->isDefeated();
    if (EndDragonSpawnPolicy::shouldSpawnDragon(chunk_X, chunk_Z, defeated, hasLivingEnderDragon(currentWorld)))
    {
        EntityDragon *dragon = new EntityDragon(currentWorld);
        dragon->setLocationAndAngles(0.0, 128.0, 0.0,
                                     randomGenerator->nextFloat() * 360.0f, 0.0f);
        if (!currentWorld->spawnEntityInWorld(dragon))
            delete dragon;
    }
}
