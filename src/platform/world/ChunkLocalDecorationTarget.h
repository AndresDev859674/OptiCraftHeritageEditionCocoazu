#pragma once

#include <cstddef>
#include <vector>

#include "java/Type.h"
#include "net/minecraft/src/StructureBoundingBox.h"

// Block access for decorating a chunk that does not exist yet.
//
// With PLATFORM_CHUNK_LOCAL_DECORATION the vegetation and ore passes of
// BiomeDecorator run inside chunk generation, against the generator's flat
// block buffer, instead of in the deferred 2x2 populate against live chunks.
// While a target is bound, World routes its block reads and writes here:
// reads outside the chunk answer air, writes outside it are dropped, so a
// tree on the border is clipped the way early Pocket Edition clipped them,
// and nothing ever touches the chunk provider. Nothing is lit or meshed per
// block; the chunk gets its skylight once, after decoration, like any other
// generated chunk.
//
// Buffer layout is the generator's: index = (x << 11 | z << 7 | y), one byte
// per block for ids and one for metadata. The buffer is 128 blocks tall (the
// Beta column layout ChunkProviderGenerate still emits), NOT WorldHeight::HEIGHT:
// with y >= 128 the OR would fold y's bit 7 onto z's bit 0 and every write
// would land in an odd column. Everything above BUFFER_HEIGHT is air here.
class ChunkLocalDecorationTarget
{
public:
    static constexpr int_t BUFFER_HEIGHT = 128;
    static constexpr std::size_t BLOCK_COUNT = 16 * 16 * BUFFER_HEIGHT;

    bool isActive() const { return blocks != nullptr; }
    void bind(int_t chunkX, int_t chunkZ, byte_t *blockIds, byte_t *blockMetadata,
              const std::vector<StructureBoundingBox> *structureBounds = nullptr);
    void reset();
    void setStructureAvoidanceEnabled(bool enabled) { structureAvoidanceEnabled = enabled; }

    bool contains(int_t x, int_t z) const;

    int_t getBlockId(int_t x, int_t y, int_t z) const;
    int_t getBlockMetadata(int_t x, int_t y, int_t z) const;
    // False when the position lies outside the chunk (write dropped).
    bool setBlock(int_t x, int_t y, int_t z, int_t blockId, int_t metadata);

    // Chunk::getHeightValue equivalent: one above the highest block with a
    // non-zero light opacity, 0 for an empty column. Outside the chunk: 0.
    int_t getHeightValue(int_t x, int_t z) const;
    // World::getTopSolidOrLiquidBlock equivalent: one above the highest block
    // whose material is solid and not leaves. Outside the chunk: -1.
    int_t getTopSolidOrLiquidBlock(int_t x, int_t z) const;

private:
    std::size_t index(int_t localX, int_t y, int_t localZ) const
    {
        return static_cast<std::size_t>(localX << 11 | localZ << 7 | y);
    }

    int_t baseX = 0;
    int_t baseZ = 0;
    byte_t *blocks = nullptr;
    bool isStructureReserved(int_t x, int_t y, int_t z) const;

    byte_t *metadata = nullptr;
    const std::vector<StructureBoundingBox> *structureBounds = nullptr;
    bool structureAvoidanceEnabled = false;
};
