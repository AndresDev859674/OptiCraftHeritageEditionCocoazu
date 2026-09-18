#include "platform/world/ChunkLocalDecorationTarget.h"

#include "net/minecraft/src/Block.h"
#include "net/minecraft/src/Material.h"
#include "platform/world/StructureReservation.h"

void ChunkLocalDecorationTarget::bind(int_t chunkX, int_t chunkZ, byte_t *blockIds, byte_t *blockMetadata,
                                      const std::vector<StructureBoundingBox> *reservedStructureBounds)
{
    baseX = chunkX << 4;
    baseZ = chunkZ << 4;
    blocks = blockIds;
    metadata = blockMetadata;
    structureBounds = reservedStructureBounds;
    structureAvoidanceEnabled = false;
}

void ChunkLocalDecorationTarget::reset()
{
    blocks = nullptr;
    metadata = nullptr;
    structureBounds = nullptr;
    structureAvoidanceEnabled = false;
}

bool ChunkLocalDecorationTarget::contains(int_t x, int_t z) const
{
    return blocks != nullptr &&
           x >= baseX && x < baseX + 16 &&
           z >= baseZ && z < baseZ + 16;
}

bool ChunkLocalDecorationTarget::isStructureReserved(int_t x, int_t y, int_t z) const
{
    return structureAvoidanceEnabled && structureBounds != nullptr &&
           StructureReservation::contains(*structureBounds, x, y, z);
}

int_t ChunkLocalDecorationTarget::getBlockId(int_t x, int_t y, int_t z) const
{
    if (y < 0 || y >= BUFFER_HEIGHT || !contains(x, z))
        return 0;
    if (isStructureReserved(x, y, z))
        return Block::stone->blockID;
    return blocks[index(x - baseX, y, z - baseZ)] & 0xff;
}

int_t ChunkLocalDecorationTarget::getBlockMetadata(int_t x, int_t y, int_t z) const
{
    if (y < 0 || y >= BUFFER_HEIGHT || !contains(x, z))
        return 0;
    return metadata[index(x - baseX, y, z - baseZ)] & 0xf;
}

bool ChunkLocalDecorationTarget::setBlock(int_t x, int_t y, int_t z, int_t blockId, int_t blockMetadata)
{
    if (y < 0 || y >= BUFFER_HEIGHT || !contains(x, z))
        return false;
    if (blockId < 0 || blockId >= Block::BLOCK_REGISTRY_SIZE || isStructureReserved(x, y, z))
        return false;
    const std::size_t at = index(x - baseX, y, z - baseZ);
    blocks[at] = static_cast<byte_t>(blockId);
    metadata[at] = static_cast<byte_t>(blockMetadata & 0xf);
    return true;
}

int_t ChunkLocalDecorationTarget::getHeightValue(int_t x, int_t z) const
{
    if (!contains(x, z))
        return 0;
    const byte_t *column = blocks + index(x - baseX, 0, z - baseZ);
    for (int_t y = BUFFER_HEIGHT - 1; y >= 0; --y)
    {
        const int_t blockId = column[y] & 0xff;
        if (blockId != 0 && Block::lightOpacity[blockId] != 0)
            return y + 1;
    }
    return 0;
}

int_t ChunkLocalDecorationTarget::getTopSolidOrLiquidBlock(int_t x, int_t z) const
{
    if (!contains(x, z))
        return -1;
    const byte_t *column = blocks + index(x - baseX, 0, z - baseZ);
    for (int_t y = BUFFER_HEIGHT - 1; y > 0; --y)
    {
        const int_t blockId = column[y] & 0xff;
        if (blockId <= 0)
            continue;
        Block *block = Block::blocksList[blockId];
        if (block != nullptr && block->blockMaterial->getIsSolid() && block->blockMaterial != Material::leaves)
            return y + 1;
    }
    return -1;
}
