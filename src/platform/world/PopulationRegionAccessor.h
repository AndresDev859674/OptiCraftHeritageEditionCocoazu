#pragma once

#include "java/Type.h"
#include "net/minecraft/src/WorldHeight.h"

class Chunk;
class ExtendedBlockStorage;

class PopulationRegionAccessor
{
public:
    static constexpr int_t CHUNK_AXIS = 2;
    static constexpr int_t CHUNK_COUNT = CHUNK_AXIS * CHUNK_AXIS;

    void reset();
    void bind(int_t baseChunkX, int_t baseChunkZ, Chunk *const chunks[CHUNK_COUNT]);

    Chunk *getChunk(int_t x, int_t y, int_t z) const;
    bool tryGetBlockId(int_t x, int_t y, int_t z, int_t &blockId);

private:
    int_t blockIndex(int_t x, int_t y, int_t z, int_t &chunkIndex,
                     int_t &sectionIndex, int_t &localX, int_t &localY, int_t &localZ) const;

    bool active = false;
    int_t baseBlockX = 0;
    int_t baseBlockZ = 0;
    Chunk *chunks[CHUNK_COUNT] = {};
    ExtendedBlockStorage *sections[CHUNK_COUNT][WorldHeight::SECTION_COUNT] = {};
};
