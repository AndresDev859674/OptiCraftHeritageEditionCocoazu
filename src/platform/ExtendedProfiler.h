#pragma once

#include "platform/Log.h"
#include "platform/PlatformConfig.h"

#include <cstddef>
#include <cstdint>

class Entity;
class WorldGenerator;

enum class PlatformEntityCullReason
{
    Distance = 0,
    Range,
    Frustum,
    SelfHidden,
    MissingChunk,
    Count
};

enum class PlatformDrawCategory
{
    Terrain = 0,
    Entities,
    Particles,
    Hand,
    Gui,
    Selection,
    Weather,
    Clouds,
    Count
};

enum class PlatformMeshStage
{
    SourceGate = 0,
    Greedy,
    BlockScan,
    Capture,
    Publish,
    Count
};

enum class PlatformMeshResetReason
{
    DirtyRestart = 0,
    SourceChanged,
    SourceGenerated,
    PendingSource,
    Recycle,
    Completion,
    Count
};

enum class PlatformChunkDecodeStage
{
    Blocks = 0,
    Lighting,
    Entities,
    TileEntities,
    TileTicks,
    Count
};

enum class PlatformPopulationAccessKind
{
    BlockRead = 0,
    ChunkLookup,
    BlockWrite,
    Count
};

enum class PlatformPopulationFastPathStage
{
    LightingFlush = 0,
    SkylightRegen,
    Count
};

struct PlatformPopulationAccessSnapshot
{
    unsigned long reads = 0;
    unsigned long readFast = 0;
    unsigned long chunkLookups = 0;
    unsigned long chunkFast = 0;
    unsigned long writes = 0;
    unsigned long writeFast = 0;
};

struct PlatformDrawSnapshot
{
    long drawCalls = 0;
    long vertices = 0;
    long vu0Vertices = 0;
    long vu1Vertices = 0;
};

#if PLATFORM_PS2 && MC_LOG_LEVEL > 2
PlatformDrawSnapshot platformProfileDrawSnapshot();
void platformProfileEntityTick(std::uint32_t start, Entity *entity);
void platformProfileEntityDrawDetailed(std::uint32_t start, const PlatformDrawSnapshot &drawStart, Entity *entity);
void platformProfileEntityFrame(int loadedEntities);
void platformProfileEntityCandidate();
void platformProfileEntityCull(PlatformEntityCullReason reason);
void platformProfileEntityRendered();
void platformProfileEntitySpecialBypass();
void platformProfileDrawCategory(PlatformDrawCategory category, const PlatformDrawSnapshot &start);
void platformProfileMeshStage(std::uint32_t start, PlatformMeshStage stage);
void platformProfileMeshReset(PlatformMeshResetReason reason);
void platformProfileMeshScheduler(int attempts, int steps, int completed, int active, int pending);
void platformProfileTextureLookup(const char *path, bool hit, bool loaded, std::uint32_t loadCycles);
void platformProfileChunkDecode(std::uint32_t start, PlatformChunkDecodeStage stage);
PlatformPopulationAccessSnapshot platformProfilePopulationAccessSnapshot();
void platformProfilePopulationAccess(PlatformPopulationAccessKind kind, bool fastPath);
void platformProfileTreeGenerator(std::uint32_t start, const PlatformPopulationAccessSnapshot &accessStart, WorldGenerator *generator);
void platformProfilePopulationFastPathStage(std::uint32_t start, PlatformPopulationFastPathStage stage, int operations);
void platformProfileAllocation(std::size_t bytes);
void platformProfileModelCacheRetain(bool reused);
void platformProfileModelCacheGet(bool hit);
void platformProfileGsQueue(long usedBytes, long capacityBytes);
void platformLogExtendedProfileAndReset(int frame);
#else
inline PlatformDrawSnapshot platformProfileDrawSnapshot() { return PlatformDrawSnapshot(); }
inline void platformProfileEntityTick(std::uint32_t, Entity *) {}
inline void platformProfileEntityDrawDetailed(std::uint32_t, const PlatformDrawSnapshot &, Entity *) {}
inline void platformProfileEntityFrame(int) {}
inline void platformProfileEntityCandidate() {}
inline void platformProfileEntityCull(PlatformEntityCullReason) {}
inline void platformProfileEntityRendered() {}
inline void platformProfileEntitySpecialBypass() {}
inline void platformProfileDrawCategory(PlatformDrawCategory, const PlatformDrawSnapshot &) {}
inline void platformProfileMeshStage(std::uint32_t, PlatformMeshStage) {}
inline void platformProfileMeshReset(PlatformMeshResetReason) {}
inline void platformProfileMeshScheduler(int, int, int, int, int) {}
inline void platformProfileTextureLookup(const char *, bool, bool, std::uint32_t) {}
inline void platformProfileChunkDecode(std::uint32_t, PlatformChunkDecodeStage) {}
inline PlatformPopulationAccessSnapshot platformProfilePopulationAccessSnapshot() { return PlatformPopulationAccessSnapshot(); }
inline void platformProfilePopulationAccess(PlatformPopulationAccessKind, bool) {}
inline void platformProfileTreeGenerator(std::uint32_t, const PlatformPopulationAccessSnapshot &, WorldGenerator *) {}
inline void platformProfilePopulationFastPathStage(std::uint32_t, PlatformPopulationFastPathStage, int) {}
inline void platformProfileAllocation(std::size_t) {}
inline void platformProfileModelCacheRetain(bool) {}
inline void platformProfileModelCacheGet(bool) {}
inline void platformProfileGsQueue(long, long) {}
inline void platformLogExtendedProfileAndReset(int) {}
#endif
