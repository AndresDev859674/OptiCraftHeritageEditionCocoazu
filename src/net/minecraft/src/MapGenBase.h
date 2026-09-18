#pragma once

#include "java/Random.h"
#include "java/Type.h"

class IChunkProvider;
class World;
class WorldChunkManager;

// net.minecraft.src.MapGenBase
class MapGenBase
{
public:
    MapGenBase();
    virtual ~MapGenBase() = default;

    virtual void generate(IChunkProvider *provider, World *world,
                          int_t chunkX, int_t chunkZ, byte_t blocks[]);

    void setWorldChunkManagerOverride(WorldChunkManager *manager);

    // Resumable form of generate(). Runs at most sourceBudget source columns of
    // the sweep starting at cursor, advances cursor, and returns true once the
    // whole sweep is done. sourceBudget <= 0 means no limit, which is how
    // generate() itself is implemented.
    //
    // Slicing here is exact rather than approximate. Every source column seeds
    // rand from (sourceColumn, worldSeed) alone and shares no state with its
    // neighbours, so a sweep that is suspended and resumed writes the same blocks
    // as one that ran to completion -- unlike the float/octave trade-offs
    // elsewhere in the console profile, this cannot move a cave by a block.
    //
    // The caller owns cursor and must keep it with the target block buffer, since
    // the two together are the whole state of a partially applied sweep.
    bool generateRange(World *world, int_t chunkX, int_t chunkZ, byte_t blocks[],
                       int_t &cursor, int_t sourceBudget);

    // One source walk shared by several generators with the same sourceRange.
    //
    // The per-source seed is a function of the world seed and the source column
    // only (see sourceSeed()), so every generator in the list would derive the
    // same value; here it is computed once, and only when at least one generator
    // still needs the column (sourceNeedsGeneration()). Generators are visited in
    // list order for each column. Same slicing contract as generateRange().
    static bool generateRangeShared(World *world, int_t chunkX, int_t chunkZ, byte_t blocks[],
                                    int_t &cursor, int_t sourceBudget,
                                    MapGenBase *const *generators, int_t generatorCount);

protected:
    virtual void generateChunk(World *world, int_t sourceChunkX, int_t sourceChunkZ,
                               int_t targetChunkX, int_t targetChunkZ, byte_t blocks[]);
    // False when generateChunk() would return without drawing from rand for this
    // source column, so a shared sweep can skip seeding it. Default: always true.
    virtual bool sourceNeedsGeneration(int_t sourceChunkX, int_t sourceChunkZ);
    WorldChunkManager *getWorldChunkManager() const;

    // Refreshes the cached seed multipliers for world's seed and returns that seed.
    long_t prepareSourceSeeding(World *world);
    long_t sourceSeed(int_t sourceChunkX, int_t sourceChunkZ, long_t worldSeed) const;

    int_t sourceRange;
    int_t range;
    Random rand;
    World *worldObj;
    WorldChunkManager *worldChunkManagerOverride;
    bool cachedSeedValid;
    long_t cachedWorldSeed;
    long_t cachedXMultiplier;
    long_t cachedZMultiplier;
};
