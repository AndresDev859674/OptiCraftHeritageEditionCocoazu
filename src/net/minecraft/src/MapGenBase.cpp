#include "MapGenBase.h"

#include "World.h"
#include "WorldChunkManager.h"
#include "java/Arithmetic.h"
#include "platform/PlatformTuning.h"

static_assert(PLATFORM_CAVE_SOURCE_RADIUS >= 0 && PLATFORM_CAVE_SOURCE_RADIUS <= 8,
              "Map generator source radius must stay within the vanilla sweep");

// A node's walk length is (range * 16 - 16) steps (MapGenCaves/MapGenRavine),
// vanilla 112 blocks at range 8. With a reduced source radius the origins beyond
// it are never seeded, so a node walking seven chunks from a three-chunk source
// only pays sin/cos, the water probe and the distance test on ground it can
// never reach. Tie the walk to the sweep, capped at the vanilla value so the
// desktop profile (source radius 8) is unchanged.
MapGenBase::MapGenBase()
    : sourceRange(PLATFORM_CAVE_SOURCE_RADIUS),
      range(PLATFORM_CAVE_SOURCE_RADIUS + 1 < 8 ? PLATFORM_CAVE_SOURCE_RADIUS + 1 : 8),
      worldObj(nullptr),
      worldChunkManagerOverride(nullptr),
      cachedSeedValid(false),
      cachedWorldSeed(0),
      cachedXMultiplier(0),
      cachedZMultiplier(0)
{
}

long_t MapGenBase::prepareSourceSeeding(World *world)
{
    worldObj = world;
    const long_t worldSeed = world->getRandomSeed();
    if (!cachedSeedValid || cachedWorldSeed != worldSeed)
    {
        rand.setSeed(worldSeed);
        cachedWorldSeed = worldSeed;
        cachedXMultiplier = rand.nextLong();
        cachedZMultiplier = rand.nextLong();
        cachedSeedValid = true;
    }
    return worldSeed;
}

long_t MapGenBase::sourceSeed(int_t sourceChunkX, int_t sourceChunkZ, long_t worldSeed) const
{
    const long_t xSeed = JavaArithmetic::longMul(static_cast<long_t>(sourceChunkX), cachedXMultiplier);
    const long_t zSeed = JavaArithmetic::longMul(static_cast<long_t>(sourceChunkZ), cachedZMultiplier);
    const ulong_t seedBits = static_cast<ulong_t>(xSeed) ^
                             static_cast<ulong_t>(zSeed) ^
                             static_cast<ulong_t>(worldSeed);
    return JavaArithmetic::longFromBits(seedBits);
}

bool MapGenBase::sourceNeedsGeneration(int_t, int_t)
{
    return true;
}

bool MapGenBase::generateRange(World *world, int_t chunkX, int_t chunkZ, byte_t blocks[],
                               int_t &cursor, int_t sourceBudget)
{
    const long_t worldSeed = prepareSourceSeeding(world);

    const int_t axis = JavaArithmetic::intAdd(JavaArithmetic::intMul(sourceRange, 2), 1);
    const int_t columnCount = JavaArithmetic::intMul(axis, axis);
    const int_t minSourceX = JavaArithmetic::intSub(chunkX, sourceRange);
    const int_t minSourceZ = JavaArithmetic::intSub(chunkZ, sourceRange);

    if (cursor < 0)
        cursor = 0;

    int_t generated = 0;
    while (cursor < columnCount && (sourceBudget <= 0 || generated < sourceBudget))
    {
        // Row-major over sourceX then sourceZ, the order the unsliced sweep used.
        const int_t sourceX = JavaArithmetic::intAdd(minSourceX, cursor / axis);
        const int_t sourceZ = JavaArithmetic::intAdd(minSourceZ, cursor % axis);

        rand.setSeed(sourceSeed(sourceX, sourceZ, worldSeed));
        generateChunk(world, sourceX, sourceZ, chunkX, chunkZ, blocks);

        cursor = JavaArithmetic::intAdd(cursor, 1);
        generated = JavaArithmetic::intAdd(generated, 1);
    }

    return cursor >= columnCount;
}

bool MapGenBase::generateRangeShared(World *world, int_t chunkX, int_t chunkZ, byte_t blocks[],
                                     int_t &cursor, int_t sourceBudget,
                                     MapGenBase *const *generators, int_t generatorCount)
{
    if (generatorCount <= 0)
        return true;

    MapGenBase *first = generators[0];
    const long_t worldSeed = first->prepareSourceSeeding(world);
    for (int_t index = 1; index < generatorCount; ++index)
    {
        MapGenBase *generator = generators[index];
        generator->prepareSourceSeeding(world);
        // The multipliers are the first two nextLong() draws of a Random seeded
        // with the world seed, so they match by construction; sourceRange is what
        // a caller could actually get wrong.
        if (generator->sourceRange != first->sourceRange ||
            generator->cachedXMultiplier != first->cachedXMultiplier ||
            generator->cachedZMultiplier != first->cachedZMultiplier)
        {
            // Not shareable: run each sweep on its own, unsliced.
            for (int_t each = 0; each < generatorCount; ++each)
            {
                int_t ownCursor = 0;
                generators[each]->generateRange(world, chunkX, chunkZ, blocks, ownCursor, 0);
            }
            return true;
        }
    }

    const int_t sourceRange = first->sourceRange;
    const int_t axis = JavaArithmetic::intAdd(JavaArithmetic::intMul(sourceRange, 2), 1);
    const int_t columnCount = JavaArithmetic::intMul(axis, axis);
    const int_t minSourceX = JavaArithmetic::intSub(chunkX, sourceRange);
    const int_t minSourceZ = JavaArithmetic::intSub(chunkZ, sourceRange);

    if (cursor < 0)
        cursor = 0;

    int_t generated = 0;
    while (cursor < columnCount && (sourceBudget <= 0 || generated < sourceBudget))
    {
        const int_t sourceX = JavaArithmetic::intAdd(minSourceX, cursor / axis);
        const int_t sourceZ = JavaArithmetic::intAdd(minSourceZ, cursor % axis);

        bool seeded = false;
        long_t seed = 0;
        for (int_t index = 0; index < generatorCount; ++index)
        {
            MapGenBase *generator = generators[index];
            if (!generator->sourceNeedsGeneration(sourceX, sourceZ))
                continue;
            if (!seeded)
            {
                seed = first->sourceSeed(sourceX, sourceZ, worldSeed);
                seeded = true;
            }
            generator->rand.setSeed(seed);
            generator->generateChunk(world, sourceX, sourceZ, chunkX, chunkZ, blocks);
        }

        cursor = JavaArithmetic::intAdd(cursor, 1);
        generated = JavaArithmetic::intAdd(generated, 1);
    }

    return cursor >= columnCount;
}

void MapGenBase::setWorldChunkManagerOverride(WorldChunkManager *manager)
{
    worldChunkManagerOverride = manager;
}

WorldChunkManager *MapGenBase::getWorldChunkManager() const
{
    if (worldChunkManagerOverride != nullptr)
        return worldChunkManagerOverride;
    return worldObj != nullptr ? worldObj->getWorldChunkManager() : nullptr;
}

void MapGenBase::generate(IChunkProvider *, World *world,
                          int_t chunkX, int_t chunkZ, byte_t blocks[])
{
    int_t cursor = 0;
    generateRange(world, chunkX, chunkZ, blocks, cursor, 0);
}

void MapGenBase::generateChunk(World *, int_t, int_t, int_t, int_t, byte_t *)
{
}
