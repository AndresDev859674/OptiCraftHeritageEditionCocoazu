#pragma once

#include "java/Type.h"

#include "IChunkLoader.h"
#include <memory>
#include <string>
#include <vector>

class World;
class Chunk;
class NBTTagCompound;

// net.minecraft.src.McRegionChunkLoader
class McRegionChunkLoader : public IChunkLoader
{
public:
    explicit McRegionChunkLoader(const std::string &worldDir);

    Chunk* loadChunk(World *world, int_t x, int_t z,
                     ChunkLoadStatus *status = nullptr) override;
    bool readChunkData(int_t x, int_t z, std::vector<byte_t> &data,
                       ChunkLoadStatus *status = nullptr);
    Chunk* loadChunkFromData(World *world, int_t x, int_t z,
                             std::vector<byte_t> &data,
                             ChunkLoadStatus *status = nullptr);
    // loadChunkFromData in two halves, for a streaming worker. The first
    // parses the NBT and builds the chunk's blocks, light and heightmap (all
    // of it private to the new Chunk, like the generator's provideChunk) and
    // hands the parsed root back so the second, on the game thread, can
    // construct the entities and tile entities it holds.
    Chunk* decodeChunkBlocksFromData(World *world, int_t x, int_t z,
                                     std::vector<byte_t> &data,
                                     std::unique_ptr<NBTTagCompound> &rootOut,
                                     ChunkLoadStatus *status = nullptr);
    static void attachChunkEntities(World *world, Chunk *chunk, NBTTagCompound *root);
    void saveChunk(World *world, Chunk *chunk) override;
    void saveExtraChunkData(World *world, Chunk *chunk) override;
    void addRandomArmor() override;
    void saveExtraData() override;

private:
    std::string worldDir;
    // Synchronous I/O scratch, retained for the loader lifetime.  A beta
    // chunk's NBT typically fits in 85KB; reusing these removes the repeated
    // grow/free cycles that fragment newlib's heap during world streaming.
    std::vector<byte_t> readScratch;
    std::vector<byte_t> writeScratch;
};
