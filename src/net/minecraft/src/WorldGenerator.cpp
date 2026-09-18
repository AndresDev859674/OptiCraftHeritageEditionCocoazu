#include "WorldGenerator.h"

#include "Chunk.h"
#include "World.h"

WorldGenerator::WorldGenerator()
    : doBlockNotify(false)
{
}

WorldGenerator::WorldGenerator(bool notify)
    : doBlockNotify(notify)
{
}

void WorldGenerator::setBlock(World *world, int_t x, int_t y, int_t z, int_t blockId)
{
    setBlockAndMetadata(world, x, y, z, blockId, 0);
}

void WorldGenerator::setBlockAndMetadata(World *world, int_t x, int_t y, int_t z,
                                         int_t blockId, int_t metadata)
{
    if (doBlockNotify)
    {
        world->setBlockAndMetadataWithNotify(x, y, z, blockId, metadata);
        return;
    }

    Chunk *chunk = world->getChunkForPopulation(x, y, z);

    if (chunk != nullptr && chunk->field_50120_o)
    {
        if (world->setBlockAndMetadataForPopulation(x, y, z, blockId, metadata))
            world->markBlockNeedsUpdate(x, y, z);
        return;
    }

    world->setBlockAndMetadataForPopulation(x, y, z, blockId, metadata);
}
