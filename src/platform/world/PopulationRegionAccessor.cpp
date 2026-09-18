#include "platform/world/PopulationRegionAccessor.h"

#include <algorithm>

#include "java/Arithmetic.h"
#include "net/minecraft/src/Chunk.h"
#include "net/minecraft/src/ExtendedBlockStorage.h"

void PopulationRegionAccessor::reset()
{
    active = false;
    baseBlockX = 0;
    baseBlockZ = 0;
    std::fill(&chunks[0], &chunks[0] + CHUNK_COUNT, nullptr);
    for (int_t chunkIndex = 0; chunkIndex < CHUNK_COUNT; ++chunkIndex)
        std::fill(&sections[chunkIndex][0],
                  &sections[chunkIndex][0] + WorldHeight::SECTION_COUNT, nullptr);
}

void PopulationRegionAccessor::bind(int_t baseChunkX, int_t baseChunkZ,
                                    Chunk *const sourceChunks[CHUNK_COUNT])
{
    reset();
    baseBlockX = JavaArithmetic::intMul(baseChunkX, 16);
    baseBlockZ = JavaArithmetic::intMul(baseChunkZ, 16);

    for (int_t chunkIndex = 0; chunkIndex < CHUNK_COUNT; ++chunkIndex)
    {
        Chunk *chunk = sourceChunks[chunkIndex];
        if (chunk == nullptr)
        {
            reset();
            return;
        }

        chunks[chunkIndex] = chunk;
        ExtendedBlockStorage *const *storage = chunk->getBlockStorageArray();
        for (int_t sectionIndex = 0; sectionIndex < WorldHeight::SECTION_COUNT; ++sectionIndex)
            sections[chunkIndex][sectionIndex] = storage[sectionIndex];
    }

    active = true;
}

int_t PopulationRegionAccessor::blockIndex(int_t x, int_t y, int_t z, int_t &chunkIndex,
                                           int_t &sectionIndex, int_t &localX,
                                           int_t &localY, int_t &localZ) const
{
    if (!active || y < WorldHeight::MIN_Y || y >= WorldHeight::HEIGHT)
        return -1;

    const int_t relativeX = JavaArithmetic::intSub(x, baseBlockX);
    const int_t relativeZ = JavaArithmetic::intSub(z, baseBlockZ);
    if (relativeX < 0 || relativeX >= CHUNK_AXIS * 16 ||
        relativeZ < 0 || relativeZ >= CHUNK_AXIS * 16)
        return -1;

    const int_t chunkX = relativeX >> 4;
    const int_t chunkZ = relativeZ >> 4;
    chunkIndex = chunkZ * CHUNK_AXIS + chunkX;
    sectionIndex = y >> 4;
    localX = relativeX & 15;
    localY = y & 15;
    localZ = relativeZ & 15;
    return chunkIndex;
}

Chunk *PopulationRegionAccessor::getChunk(int_t x, int_t y, int_t z) const
{
    if (!active || y < WorldHeight::MIN_Y || y >= WorldHeight::HEIGHT)
        return nullptr;

    const int_t relativeX = JavaArithmetic::intSub(x, baseBlockX);
    const int_t relativeZ = JavaArithmetic::intSub(z, baseBlockZ);
    if (relativeX < 0 || relativeX >= CHUNK_AXIS * 16 ||
        relativeZ < 0 || relativeZ >= CHUNK_AXIS * 16)
        return nullptr;

    return chunks[(relativeZ >> 4) * CHUNK_AXIS + (relativeX >> 4)];
}

bool PopulationRegionAccessor::tryGetBlockId(int_t x, int_t y, int_t z, int_t &blockId)
{
    int_t chunkIndex = 0;
    int_t sectionIndex = 0;
    int_t localX = 0;
    int_t localY = 0;
    int_t localZ = 0;
    if (blockIndex(x, y, z, chunkIndex, sectionIndex, localX, localY, localZ) < 0)
        return false;

    ExtendedBlockStorage *section = sections[chunkIndex][sectionIndex];
    if (section == nullptr)
    {
        ExtendedBlockStorage *const *storage = chunks[chunkIndex]->getBlockStorageArray();
        section = storage[sectionIndex];
        sections[chunkIndex][sectionIndex] = section;
    }
    blockId = section != nullptr ? section->getExtBlockID(localX, localY, localZ) : 0;
    return true;
}
