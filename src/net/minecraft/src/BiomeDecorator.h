#pragma once

#include <memory>

#include "java/Type.h"

class BiomeGenBase;
class Random;
class World;
class WorldGenerator;

// net.minecraft.src.BiomeDecorator
class BiomeDecorator
{
public:
    explicit BiomeDecorator(BiomeGenBase *biome);
    virtual ~BiomeDecorator();

    // Which stages a decoration run covers. Vegetation is everything that only
    // writes blocks (ores through cacti) and can run against a chunk buffer;
    // Springs is the two liquid passes, which need block updates on a live
    // world. Full is both, the vanilla run.
    enum class DecorationPass
    {
        Full,
        Vegetation,
        Springs
    };

    void decorate(World *world, Random &random, int_t chunkX, int_t chunkZ);
    void beginDecoration(World *world, Random &random, int_t chunkX, int_t chunkZ,
                         DecorationPass pass = DecorationPass::Full);
    bool advanceDecoration();
    void finishDecoration();

    bool generateLakes;

protected:
    virtual void decorate();
    void genStandardOre1(int_t count, WorldGenerator *generator, int_t minY, int_t maxY);
    void genStandardOre2(int_t count, WorldGenerator *generator, int_t centerY, int_t spread);
    void generateOres();

    enum class DecorationStage
    {
        Dirt,
        Gravel,
        Coal,
        Iron,
        Gold,
        Redstone,
        Diamond,
        Lapis,
        Sand,
        Clay,
        GravelAsSand,
        TreeSetup,
        Trees,
        BigMushrooms,
        YellowFlowers,
        RedFlowers,
        Grass,
        DeadBushes,
        WaterLilies,
        MushroomLoopBrown,
        MushroomLoopRed,
        BrownMushroom,
        RedMushroom,
        Reeds,
        ExtraReeds,
        Pumpkin,
        Cacti,
        WaterSprings,
        LavaSprings,
        Done
    };

    void nextStage(DecorationStage stage);
    static const char *stageName(DecorationStage stage);
    bool advanceStandardOre1(int_t count, WorldGenerator *generator, int_t minY, int_t maxY);
    bool advanceStandardOre2(int_t count, WorldGenerator *generator, int_t centerY, int_t spread);

    World *currentWorld;
    Random *randomGenerator;
    int_t chunk_X;
    int_t chunk_Z;
    BiomeGenBase *biome;
    DecorationStage decorationStage;
    // The run ends when it reaches this stage; Done for a full run.
    DecorationStage stopStage;
    int_t decorationIndex;
    int_t treeCount;

    std::unique_ptr<WorldGenerator> clayGen;
    std::unique_ptr<WorldGenerator> sandGen;
    std::unique_ptr<WorldGenerator> gravelAsSandGen;
    std::unique_ptr<WorldGenerator> dirtGen;
    std::unique_ptr<WorldGenerator> gravelGen;
    std::unique_ptr<WorldGenerator> coalGen;
    std::unique_ptr<WorldGenerator> ironGen;
    std::unique_ptr<WorldGenerator> goldGen;
    std::unique_ptr<WorldGenerator> redstoneGen;
    std::unique_ptr<WorldGenerator> diamondGen;
    std::unique_ptr<WorldGenerator> lapisGen;
    std::unique_ptr<WorldGenerator> plantYellowGen;
    std::unique_ptr<WorldGenerator> plantRedGen;
    std::unique_ptr<WorldGenerator> mushroomBrownGen;
    std::unique_ptr<WorldGenerator> mushroomRedGen;
    std::unique_ptr<WorldGenerator> bigMushroomGen;
    std::unique_ptr<WorldGenerator> reedGen;
    std::unique_ptr<WorldGenerator> cactusGen;
    std::unique_ptr<WorldGenerator> waterlilyGen;

public:
    int_t waterlilyPerChunk;
    int_t treesPerChunk;
    int_t flowersPerChunk;
    int_t grassPerChunk;
    int_t deadBushPerChunk;
    int_t mushroomsPerChunk;
    int_t reedsPerChunk;
    int_t cactiPerChunk;
    int_t sandPerChunk;
    int_t sandPerChunk2;
    int_t clayPerChunk;
    int_t bigMushroomsPerChunk;
};
