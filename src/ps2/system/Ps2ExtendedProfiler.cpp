#include "platform/ExtendedProfiler.h"

#if PLATFORM_PS2 && MC_LOG_LEVEL > 2

#include "net/minecraft/src/Entity.h"
#include "net/minecraft/src/EntityList.h"
#include "net/minecraft/src/WorldGenBigTree.h"
#include "net/minecraft/src/WorldGenForest.h"
#include "net/minecraft/src/WorldGenHugeTrees.h"
#include "net/minecraft/src/WorldGenShrub.h"
#include "net/minecraft/src/WorldGenSwamp.h"
#include "net/minecraft/src/WorldGenTaiga1.h"
#include "net/minecraft/src/WorldGenTaiga2.h"
#include "net/minecraft/src/WorldGenTrees.h"
#include "net/minecraft/src/WorldGenerator.h"
#include "platform/Profiler.h"
#include "ps2/render/Ps2RenderStats.h"
#include "ps2/render/Ps2GsQueue.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <typeinfo>

namespace
{
struct CycleSample
{
    unsigned long long cycles = 0;
    std::uint32_t maxCycles = 0;
    unsigned int count = 0;
};

struct EntityProfile
{
    const std::type_info *type = nullptr;
    char name[40] = {};
    CycleSample tick;
    CycleSample draw;
    unsigned long drawCalls = 0;
    unsigned long vertices = 0;
    unsigned long vu0Vertices = 0;
    unsigned long vu1Vertices = 0;
};

struct DrawProfile
{
    unsigned long calls = 0;
    unsigned long vertices = 0;
    unsigned long vu0Vertices = 0;
    unsigned long vu1Vertices = 0;
    unsigned int samples = 0;
};

struct TextureProfile
{
    char path[72] = {};
    CycleSample load;
    unsigned int failures = 0;
};

struct TreeProfile
{
    CycleSample generate;
    unsigned long reads = 0;
    unsigned long readFast = 0;
    unsigned long chunkLookups = 0;
    unsigned long chunkFast = 0;
    unsigned long writes = 0;
    unsigned long writeFast = 0;
};

constexpr int kEntitySlots = 48;
constexpr int kReportedEntities = 10;
constexpr int kTextureSlots = 24;
constexpr int kReportedTextures = 8;
constexpr int kGsSamples = 128;
constexpr int kTreeKinds = 9;

EntityProfile s_entities[kEntitySlots];
int s_entityCount = 0;
EntityProfile s_entityOverflow;

unsigned long s_entityFrames = 0;
unsigned long s_entityLoaded = 0;
unsigned long s_entityCandidates = 0;
unsigned long s_entityRendered = 0;
unsigned long s_entitySpecialBypass = 0;
unsigned long s_entityCull[static_cast<int>(PlatformEntityCullReason::Count)] = {};

DrawProfile s_draw[static_cast<int>(PlatformDrawCategory::Count)];
CycleSample s_meshStage[static_cast<int>(PlatformMeshStage::Count)];
unsigned long s_meshReset[static_cast<int>(PlatformMeshResetReason::Count)] = {};
unsigned long s_meshAttempts = 0;
unsigned long s_meshSteps = 0;
unsigned long s_meshCompleted = 0;
unsigned long s_meshActiveSum = 0;
unsigned long s_meshPendingSum = 0;
unsigned int s_meshSchedulerSamples = 0;

TextureProfile s_textures[kTextureSlots];
int s_textureCount = 0;
unsigned long s_textureHits = 0;
unsigned long s_textureMisses = 0;
unsigned long s_textureFailures = 0;

CycleSample s_chunkDecode[static_cast<int>(PlatformChunkDecodeStage::Count)];

PlatformPopulationAccessSnapshot s_populationAccess;
TreeProfile s_tree[kTreeKinds];
CycleSample s_populationFastPath[static_cast<int>(PlatformPopulationFastPathStage::Count)];
unsigned long s_populationFastPathOps[static_cast<int>(PlatformPopulationFastPathStage::Count)] = {};

unsigned long s_allocCalls = 0;
unsigned long long s_allocBytes = 0;
std::size_t s_allocMax = 0;

unsigned long s_modelRetainReuse = 0;
unsigned long s_modelRetainNew = 0;
unsigned long s_modelGetHit = 0;
unsigned long s_modelGetMiss = 0;

long s_gsSamples[kGsSamples] = {};
int s_gsSampleCount = 0;
long long s_gsSumBytes = 0;
long s_gsPeakBytes = 0;
long s_gsCapacityBytes = 0;

const char *const kCullNames[] = { "distance", "range", "frustum", "self", "missingChunk" };
const char *const kDrawNames[] = { "terrain", "entities", "particles", "hand", "gui", "selection", "weather", "clouds" };
const char *const kMeshStageNames[] = { "source", "greedy", "scan", "capture", "publish" };
const char *const kMeshResetNames[] = { "dirty", "sourceChanged", "sourceGenerated", "pendingSource", "recycle", "complete" };
const char *const kDecodeNames[] = { "blocks", "lighting", "entities", "tileEntities", "tileTicks" };
const char *const kTreeNames[kTreeKinds] = {
    "Trees", "BigTree", "Forest", "Swamp", "Taiga1", "Taiga2", "HugeTree", "Shrub", "Other"
};
const char *const kPopulationFastPathNames[] = { "lightingFlush", "skylightRegen" };

void add(CycleSample &sample, std::uint32_t cycles)
{
    sample.cycles += cycles;
    sample.maxCycles = std::max(sample.maxCycles, cycles);
    ++sample.count;
}

double cyclesToMs(unsigned long long cycles)
{
    return static_cast<double>(cycles) / 294000.0;
}

EntityProfile &entitySlot(Entity *entity)
{
    if (entity == nullptr)
        return s_entityOverflow;

    const std::type_info &type = typeid(*entity);
    for (int i = 0; i < s_entityCount; ++i)
    {
        if (*s_entities[i].type == type)
            return s_entities[i];
    }

    if (s_entityCount >= kEntitySlots)
        return s_entityOverflow;

    EntityProfile &slot = s_entities[s_entityCount++];
    slot.type = &type;
    const std::string name = EntityList::getEntityString(entity);
    std::snprintf(slot.name, sizeof(slot.name), "%s",
                  name.empty() ? (entity->isPlayer() ? "Player" : type.name()) : name.c_str());
    return slot;
}

void mergeEntity(EntityProfile &dst, const EntityProfile &src)
{
    dst.tick.cycles += src.tick.cycles;
    dst.tick.count += src.tick.count;
    dst.tick.maxCycles = std::max(dst.tick.maxCycles, src.tick.maxCycles);
    dst.draw.cycles += src.draw.cycles;
    dst.draw.count += src.draw.count;
    dst.draw.maxCycles = std::max(dst.draw.maxCycles, src.draw.maxCycles);
    dst.drawCalls += src.drawCalls;
    dst.vertices += src.vertices;
    dst.vu0Vertices += src.vu0Vertices;
    dst.vu1Vertices += src.vu1Vertices;
}

long percentile(long *values, int count, int pct)
{
    if (count <= 0)
        return 0;
    std::sort(values, values + count);
    int index = (count * pct + 99) / 100 - 1;
    if (index < 0)
        index = 0;
    if (index >= count)
        index = count - 1;
    return values[index];
}

int treeKind(WorldGenerator *generator)
{
    if (dynamic_cast<WorldGenBigTree *>(generator) != nullptr) return 1;
    if (dynamic_cast<WorldGenForest *>(generator) != nullptr) return 2;
    if (dynamic_cast<WorldGenSwamp *>(generator) != nullptr) return 3;
    if (dynamic_cast<WorldGenTaiga1 *>(generator) != nullptr) return 4;
    if (dynamic_cast<WorldGenTaiga2 *>(generator) != nullptr) return 5;
    if (dynamic_cast<WorldGenHugeTrees *>(generator) != nullptr) return 6;
    if (dynamic_cast<WorldGenShrub *>(generator) != nullptr) return 7;
    if (dynamic_cast<WorldGenTrees *>(generator) != nullptr) return 0;
    return 8;
}

unsigned long delta(unsigned long end, unsigned long start)
{
    return end >= start ? end - start : 0;
}
}

PlatformDrawSnapshot platformProfileDrawSnapshot()
{
    Ps2RenderStats &stats = ps2_render_stats();
    PlatformDrawSnapshot snapshot;
    snapshot.drawCalls = stats.profile3DrawCalls + stats.profile3Vu1DrawCalls;
    snapshot.vertices = stats.profile3Vertices + stats.profile3Vu1Vertices;
    snapshot.vu0Vertices = stats.profile3Vu0Vertices;
    snapshot.vu1Vertices = stats.profile3Vu1Vertices;
    return snapshot;
}

void platformProfileEntityTick(std::uint32_t start, Entity *entity)
{
    add(entitySlot(entity).tick, platformProfileRenderPhaseBegin() - start);
}

void platformProfileEntityDrawDetailed(std::uint32_t start, const PlatformDrawSnapshot &drawStart, Entity *entity)
{
    EntityProfile &slot = entitySlot(entity);
    add(slot.draw, platformProfileRenderPhaseBegin() - start);
    const PlatformDrawSnapshot end = platformProfileDrawSnapshot();
    if (end.drawCalls >= drawStart.drawCalls)
        slot.drawCalls += static_cast<unsigned long>(end.drawCalls - drawStart.drawCalls);
    if (end.vertices >= drawStart.vertices)
        slot.vertices += static_cast<unsigned long>(end.vertices - drawStart.vertices);
    if (end.vu0Vertices >= drawStart.vu0Vertices)
        slot.vu0Vertices += static_cast<unsigned long>(end.vu0Vertices - drawStart.vu0Vertices);
    if (end.vu1Vertices >= drawStart.vu1Vertices)
        slot.vu1Vertices += static_cast<unsigned long>(end.vu1Vertices - drawStart.vu1Vertices);
}

void platformProfileEntityFrame(int loadedEntities)
{
    ++s_entityFrames;
    if (loadedEntities > 0)
        s_entityLoaded += static_cast<unsigned long>(loadedEntities);
}

void platformProfileEntityCandidate() { ++s_entityCandidates; }

void platformProfileEntityCull(PlatformEntityCullReason reason)
{
    const int index = static_cast<int>(reason);
    if (index >= 0 && index < static_cast<int>(PlatformEntityCullReason::Count))
        ++s_entityCull[index];
}

void platformProfileEntityRendered() { ++s_entityRendered; }
void platformProfileEntitySpecialBypass() { ++s_entitySpecialBypass; }

void platformProfileDrawCategory(PlatformDrawCategory category, const PlatformDrawSnapshot &start)
{
    const int index = static_cast<int>(category);
    if (index < 0 || index >= static_cast<int>(PlatformDrawCategory::Count))
        return;
    const PlatformDrawSnapshot end = platformProfileDrawSnapshot();
    DrawProfile &slot = s_draw[index];
    if (end.drawCalls >= start.drawCalls)
        slot.calls += static_cast<unsigned long>(end.drawCalls - start.drawCalls);
    if (end.vertices >= start.vertices)
        slot.vertices += static_cast<unsigned long>(end.vertices - start.vertices);
    if (end.vu0Vertices >= start.vu0Vertices)
        slot.vu0Vertices += static_cast<unsigned long>(end.vu0Vertices - start.vu0Vertices);
    if (end.vu1Vertices >= start.vu1Vertices)
        slot.vu1Vertices += static_cast<unsigned long>(end.vu1Vertices - start.vu1Vertices);
    ++slot.samples;
}

void platformProfileMeshStage(std::uint32_t start, PlatformMeshStage stage)
{
    const int index = static_cast<int>(stage);
    if (index >= 0 && index < static_cast<int>(PlatformMeshStage::Count))
        add(s_meshStage[index], platformProfileRenderPhaseBegin() - start);
}

void platformProfileMeshReset(PlatformMeshResetReason reason)
{
    const int index = static_cast<int>(reason);
    if (index >= 0 && index < static_cast<int>(PlatformMeshResetReason::Count))
        ++s_meshReset[index];
}

void platformProfileMeshScheduler(int attempts, int steps, int completed, int active, int pending)
{
    if (attempts > 0) s_meshAttempts += static_cast<unsigned long>(attempts);
    if (steps > 0) s_meshSteps += static_cast<unsigned long>(steps);
    if (completed > 0) s_meshCompleted += static_cast<unsigned long>(completed);
    if (active > 0) s_meshActiveSum += static_cast<unsigned long>(active);
    if (pending > 0) s_meshPendingSum += static_cast<unsigned long>(pending);
    ++s_meshSchedulerSamples;
}

void platformProfileTextureLookup(const char *path, bool hit, bool loaded, std::uint32_t loadCycles)
{
    if (hit)
    {
        ++s_textureHits;
        return;
    }

    ++s_textureMisses;
    if (!loaded)
        ++s_textureFailures;
    if (path == nullptr)
        return;

    TextureProfile *slot = nullptr;
    for (int i = 0; i < s_textureCount; ++i)
    {
        if (std::strncmp(s_textures[i].path, path, sizeof(s_textures[i].path) - 1) == 0)
        {
            slot = &s_textures[i];
            break;
        }
    }
    if (slot == nullptr && s_textureCount < kTextureSlots)
    {
        slot = &s_textures[s_textureCount++];
        std::snprintf(slot->path, sizeof(slot->path), "%s", path);
    }
    if (slot == nullptr)
        return;

    add(slot->load, loadCycles);
    if (!loaded)
        ++slot->failures;
}

void platformProfileChunkDecode(std::uint32_t start, PlatformChunkDecodeStage stage)
{
    const int index = static_cast<int>(stage);
    if (index >= 0 && index < static_cast<int>(PlatformChunkDecodeStage::Count))
        add(s_chunkDecode[index], platformProfileRenderPhaseBegin() - start);
}

PlatformPopulationAccessSnapshot platformProfilePopulationAccessSnapshot()
{
    return s_populationAccess;
}

void platformProfilePopulationAccess(PlatformPopulationAccessKind kind, bool fastPath)
{
    switch (kind)
    {
    case PlatformPopulationAccessKind::BlockRead:
        ++s_populationAccess.reads;
        if (fastPath) ++s_populationAccess.readFast;
        break;
    case PlatformPopulationAccessKind::ChunkLookup:
        ++s_populationAccess.chunkLookups;
        if (fastPath) ++s_populationAccess.chunkFast;
        break;
    case PlatformPopulationAccessKind::BlockWrite:
        ++s_populationAccess.writes;
        if (fastPath) ++s_populationAccess.writeFast;
        break;
    case PlatformPopulationAccessKind::Count:
        break;
    }
}

void platformProfileTreeGenerator(std::uint32_t start, const PlatformPopulationAccessSnapshot &accessStart, WorldGenerator *generator)
{
    const int index = treeKind(generator);
    TreeProfile &slot = s_tree[index];
    add(slot.generate, platformProfileRenderPhaseBegin() - start);

    const PlatformPopulationAccessSnapshot accessEnd = platformProfilePopulationAccessSnapshot();
    slot.reads += delta(accessEnd.reads, accessStart.reads);
    slot.readFast += delta(accessEnd.readFast, accessStart.readFast);
    slot.chunkLookups += delta(accessEnd.chunkLookups, accessStart.chunkLookups);
    slot.chunkFast += delta(accessEnd.chunkFast, accessStart.chunkFast);
    slot.writes += delta(accessEnd.writes, accessStart.writes);
    slot.writeFast += delta(accessEnd.writeFast, accessStart.writeFast);
}

void platformProfilePopulationFastPathStage(std::uint32_t start, PlatformPopulationFastPathStage stage, int operations)
{
    const int index = static_cast<int>(stage);
    if (index < 0 || index >= static_cast<int>(PlatformPopulationFastPathStage::Count))
        return;
    add(s_populationFastPath[index], platformProfileRenderPhaseBegin() - start);
    if (operations > 0)
        s_populationFastPathOps[index] += static_cast<unsigned long>(operations);
}

void platformProfileAllocation(std::size_t bytes)
{
    ++s_allocCalls;
    s_allocBytes += bytes;
    s_allocMax = std::max(s_allocMax, bytes);
}

void platformProfileModelCacheRetain(bool reused)
{
    reused ? ++s_modelRetainReuse : ++s_modelRetainNew;
}

void platformProfileModelCacheGet(bool hit)
{
    hit ? ++s_modelGetHit : ++s_modelGetMiss;
}

void platformProfileGsQueue(long usedBytes, long capacityBytes)
{
    if (usedBytes < 0 || capacityBytes <= 0)
        return;
    s_gsSumBytes += usedBytes;
    s_gsPeakBytes = std::max(s_gsPeakBytes, usedBytes);
    s_gsCapacityBytes = capacityBytes;
    if (s_gsSampleCount < kGsSamples)
        s_gsSamples[s_gsSampleCount++] = usedBytes;
}

void platformLogExtendedProfileAndReset(int frame)
{
    if (s_entityFrames > 0)
    {
        MC_LOG_TRACE("prof3", "frame=%d entities loadedAvg=%.1f candidates=%lu rendered=%lu special=%lu cull distance=%lu range=%lu frustum=%lu self=%lu missing=%lu\n",
                     frame, static_cast<double>(s_entityLoaded) / static_cast<double>(s_entityFrames),
                     s_entityCandidates, s_entityRendered, s_entitySpecialBypass,
                     s_entityCull[0], s_entityCull[1], s_entityCull[2], s_entityCull[3], s_entityCull[4]);
    }

    int order[kEntitySlots];
    for (int i = 0; i < s_entityCount; ++i)
        order[i] = i;
    std::sort(order, order + s_entityCount, [](int a, int b) {
        return s_entities[a].draw.cycles + s_entities[a].tick.cycles >
               s_entities[b].draw.cycles + s_entities[b].tick.cycles;
    });
    EntityProfile other = s_entityOverflow;
    for (int rank = 0; rank < s_entityCount; ++rank)
    {
        EntityProfile &slot = s_entities[order[rank]];
        if (rank < kReportedEntities)
        {
            const double tickMs = cyclesToMs(slot.tick.cycles);
            const double drawMs = cyclesToMs(slot.draw.cycles);
            MC_LOG_TRACE("prof3", "frame=%d entity=%s tick n=%u total=%.2f avg=%.3f max=%.3f | draw n=%u total=%.2f avg=%.3f max=%.3f calls=%lu verts=%lu vu0=%lu vu1=%lu\n",
                         frame, slot.name,
                         slot.tick.count, tickMs, slot.tick.count ? tickMs / slot.tick.count : 0.0,
                         cyclesToMs(slot.tick.maxCycles),
                         slot.draw.count, drawMs, slot.draw.count ? drawMs / slot.draw.count : 0.0,
                         cyclesToMs(slot.draw.maxCycles), slot.drawCalls, slot.vertices,
                         slot.vu0Vertices, slot.vu1Vertices);
        }
        else
        {
            mergeEntity(other, slot);
        }
        slot.tick = CycleSample();
        slot.draw = CycleSample();
        slot.drawCalls = slot.vertices = slot.vu0Vertices = slot.vu1Vertices = 0;
    }
    if (other.tick.count || other.draw.count)
    {
        MC_LOG_TRACE("prof3", "frame=%d entity=Other tickN=%u tick=%.2fms drawN=%u draw=%.2fms calls=%lu verts=%lu\n",
                     frame, other.tick.count, cyclesToMs(other.tick.cycles), other.draw.count,
                     cyclesToMs(other.draw.cycles), other.drawCalls, other.vertices);
    }
    s_entityOverflow = EntityProfile();

    for (int i = 0; i < static_cast<int>(PlatformDrawCategory::Count); ++i)
    {
        const DrawProfile &slot = s_draw[i];
        if (slot.samples == 0)
            continue;
        MC_LOG_TRACE("prof3", "frame=%d draw=%s samples=%u calls=%lu verts=%lu vu0=%lu vu1=%lu\n",
                     frame, kDrawNames[i], slot.samples, slot.calls, slot.vertices,
                     slot.vu0Vertices, slot.vu1Vertices);
    }

    bool meshAny = false;
    for (int i = 0; i < static_cast<int>(PlatformMeshStage::Count); ++i)
        meshAny = meshAny || s_meshStage[i].count != 0;
    if (meshAny)
    {
        char line[640] = {};
        int len = std::snprintf(line, sizeof(line), "frame=%d meshStage", frame);
        for (int i = 0; i < static_cast<int>(PlatformMeshStage::Count) && len > 0 && len < (int)sizeof(line); ++i)
        {
            const CycleSample &slot = s_meshStage[i];
            if (slot.count == 0)
                continue;
            const double totalMs = cyclesToMs(slot.cycles);
            len += std::snprintf(line + len, sizeof(line) - (std::size_t)len,
                                 " %s=%.2f/avg%.3f/max%.3f/n%u",
                                 kMeshStageNames[i], totalMs, totalMs / slot.count,
                                 cyclesToMs(slot.maxCycles), slot.count);
        }
        MC_LOG_TRACE("prof3", "%s\n", line);
    }

    {
        char line[512] = {};
        int len = std::snprintf(line, sizeof(line), "frame=%d meshReset", frame);
        for (int i = 0; i < static_cast<int>(PlatformMeshResetReason::Count) && len > 0 && len < (int)sizeof(line); ++i)
            len += std::snprintf(line + len, sizeof(line) - (std::size_t)len, " %s=%lu", kMeshResetNames[i], s_meshReset[i]);
        if (s_meshSchedulerSamples > 0)
        {
            const double avgActive = static_cast<double>(s_meshActiveSum) / s_meshSchedulerSamples;
            const double avgPending = static_cast<double>(s_meshPendingSum) / s_meshSchedulerSamples;
            len += std::snprintf(line + len, sizeof(line) - (std::size_t)len,
                                 " | scheduler attempts=%lu steps=%lu complete=%lu avgActive=%.1f avgPending=%.1f stepsPerComplete=%.1f",
                                 s_meshAttempts, s_meshSteps, s_meshCompleted, avgActive, avgPending,
                                 s_meshCompleted ? static_cast<double>(s_meshSteps) / s_meshCompleted : 0.0);
        }
        MC_LOG_TRACE("prof3", "%s\n", line);
    }

    int texOrder[kTextureSlots];
    for (int i = 0; i < s_textureCount; ++i)
        texOrder[i] = i;
    std::sort(texOrder, texOrder + s_textureCount, [](int a, int b) {
        return s_textures[a].load.cycles > s_textures[b].load.cycles;
    });
    MC_LOG_TRACE("prof3", "frame=%d texture hit=%lu miss=%lu fail=%lu hitRate=%.1f%%\n",
                 frame, s_textureHits, s_textureMisses, s_textureFailures,
                 (s_textureHits + s_textureMisses) ?
                 100.0 * static_cast<double>(s_textureHits) / static_cast<double>(s_textureHits + s_textureMisses) : 0.0);
    for (int rank = 0; rank < s_textureCount && rank < kReportedTextures; ++rank)
    {
        TextureProfile &slot = s_textures[texOrder[rank]];
        const double totalMs = cyclesToMs(slot.load.cycles);
        MC_LOG_TRACE("prof3", "frame=%d textureMiss=%s n=%u total=%.2fms avg=%.2fms max=%.2fms fail=%u\n",
                     frame, slot.path, slot.load.count, totalMs,
                     slot.load.count ? totalMs / slot.load.count : 0.0,
                     cyclesToMs(slot.load.maxCycles), slot.failures);
    }

    {
        char line[512] = {};
        int len = std::snprintf(line, sizeof(line), "frame=%d chunkDecode", frame);
        for (int i = 0; i < static_cast<int>(PlatformChunkDecodeStage::Count) && len > 0 && len < (int)sizeof(line); ++i)
        {
            const CycleSample &slot = s_chunkDecode[i];
            if (slot.count == 0)
                continue;
            const double totalMs = cyclesToMs(slot.cycles);
            len += std::snprintf(line + len, sizeof(line) - (std::size_t)len,
                                 " %s=%.2f/avg%.2f/max%.2f/n%u",
                                 kDecodeNames[i], totalMs, totalMs / slot.count,
                                 cyclesToMs(slot.maxCycles), slot.count);
        }
        MC_LOG_TRACE("prof3", "%s\n", line);
    }

    if (s_populationAccess.reads || s_populationAccess.chunkLookups || s_populationAccess.writes)
    {
        MC_LOG_TRACE("prof3", "frame=%d populationAccess read=%lu fast=%lu(%.1f%%) chunk=%lu fast=%lu(%.1f%%) write=%lu fast=%lu(%.1f%%)\n",
                     frame,
                     s_populationAccess.reads, s_populationAccess.readFast,
                     s_populationAccess.reads ? 100.0 * static_cast<double>(s_populationAccess.readFast) / s_populationAccess.reads : 0.0,
                     s_populationAccess.chunkLookups, s_populationAccess.chunkFast,
                     s_populationAccess.chunkLookups ? 100.0 * static_cast<double>(s_populationAccess.chunkFast) / s_populationAccess.chunkLookups : 0.0,
                     s_populationAccess.writes, s_populationAccess.writeFast,
                     s_populationAccess.writes ? 100.0 * static_cast<double>(s_populationAccess.writeFast) / s_populationAccess.writes : 0.0);
    }

    for (int i = 0; i < kTreeKinds; ++i)
    {
        const TreeProfile &slot = s_tree[i];
        if (slot.generate.count == 0)
            continue;
        const double totalMs = cyclesToMs(slot.generate.cycles);
        MC_LOG_TRACE("prof3", "frame=%d tree=%s n=%u total=%.2fms avg=%.3fms max=%.3fms | read=%lu fast=%lu chunk=%lu fast=%lu write=%lu fast=%lu\n",
                     frame, kTreeNames[i], slot.generate.count, totalMs, totalMs / slot.generate.count,
                     cyclesToMs(slot.generate.maxCycles), slot.reads, slot.readFast,
                     slot.chunkLookups, slot.chunkFast, slot.writes, slot.writeFast);
    }

    {
        char line[320] = {};
        int len = std::snprintf(line, sizeof(line), "frame=%d populationClose", frame);
        for (int i = 0; i < static_cast<int>(PlatformPopulationFastPathStage::Count) && len > 0 && len < (int)sizeof(line); ++i)
        {
            const CycleSample &slot = s_populationFastPath[i];
            if (slot.count == 0)
                continue;
            const double totalMs = cyclesToMs(slot.cycles);
            len += std::snprintf(line + len, sizeof(line) - (std::size_t)len,
                                 " %s=%.2f/avg%.3f/max%.3f/n%u/op%lu",
                                 kPopulationFastPathNames[i], totalMs, totalMs / slot.count,
                                 cyclesToMs(slot.maxCycles), slot.count, s_populationFastPathOps[i]);
        }
        if (len > 0 && len < (int)sizeof(line) && (s_populationFastPath[0].count || s_populationFastPath[1].count))
            MC_LOG_TRACE("prof3", "%s\n", line);
    }

    const double profileFrames = s_gsSampleCount > 0 ? static_cast<double>(s_gsSampleCount) : 1.0;
    MC_LOG_TRACE("prof3", "frame=%d alloc newCalls=%lu avg=%.1f/frame bytes=%lluKB avg=%.1fKB/frame max=%uKB | model retainReuse=%lu retainNew=%lu getHit=%lu getMiss=%lu hitRate=%.1f%%\n",
                 frame, s_allocCalls, static_cast<double>(s_allocCalls) / profileFrames,
                 s_allocBytes / 1024ULL, static_cast<double>(s_allocBytes) / 1024.0 / profileFrames,
                 static_cast<unsigned int>(s_allocMax / 1024u),
                 s_modelRetainReuse, s_modelRetainNew, s_modelGetHit, s_modelGetMiss,
                 (s_modelGetHit + s_modelGetMiss) ?
                 100.0 * static_cast<double>(s_modelGetHit) / static_cast<double>(s_modelGetHit + s_modelGetMiss) : 0.0);

    if (s_gsSampleCount > 0)
    {
        long sorted[kGsSamples];
        std::copy(s_gsSamples, s_gsSamples + s_gsSampleCount, sorted);
        const long p95 = percentile(sorted, s_gsSampleCount, 95);
        Ps2GsQueueRenderStats queueStats;
        ps2_gs_queue_render_stats(queueStats, false);
        MC_LOG_TRACE("prof3", "frame=%d gsQueue avg=%ldKB p95=%ldKB peak=%ldKB cap=%ldKB samples=%d flush=%ld stall=%.2fms guardOver=%ld maxGuard=%ldKB\n",
                     frame, static_cast<long>((s_gsSumBytes / s_gsSampleCount) / 1024LL),
                     p95 / 1024, s_gsPeakBytes / 1024, s_gsCapacityBytes / 1024, s_gsSampleCount,
                     queueStats.flushes, static_cast<double>(queueStats.flushCycles) / 294000.0,
                     queueStats.guardOverruns, queueStats.maxGuardWriteBytes / 1024);
    }

    s_entityFrames = s_entityLoaded = s_entityCandidates = s_entityRendered = s_entitySpecialBypass = 0;
    std::fill(std::begin(s_entityCull), std::end(s_entityCull), 0ul);
    std::fill(std::begin(s_draw), std::end(s_draw), DrawProfile());
    std::fill(std::begin(s_meshStage), std::end(s_meshStage), CycleSample());
    std::fill(std::begin(s_meshReset), std::end(s_meshReset), 0ul);
    s_meshAttempts = s_meshSteps = s_meshCompleted = s_meshActiveSum = s_meshPendingSum = 0;
    s_meshSchedulerSamples = 0;
    s_textureHits = s_textureMisses = s_textureFailures = 0;
    std::fill(std::begin(s_textures), std::end(s_textures), TextureProfile());
    s_textureCount = 0;
    std::fill(std::begin(s_chunkDecode), std::end(s_chunkDecode), CycleSample());
    s_populationAccess = PlatformPopulationAccessSnapshot();
    std::fill(std::begin(s_tree), std::end(s_tree), TreeProfile());
    std::fill(std::begin(s_populationFastPath), std::end(s_populationFastPath), CycleSample());
    std::fill(std::begin(s_populationFastPathOps), std::end(s_populationFastPathOps), 0ul);
    s_allocCalls = 0;
    s_allocBytes = 0;
    s_allocMax = 0;
    s_modelRetainReuse = s_modelRetainNew = s_modelGetHit = s_modelGetMiss = 0;
    s_gsSampleCount = 0;
    s_gsSumBytes = 0;
    s_gsPeakBytes = 0;
    s_gsCapacityBytes = 0;

    Ps2RenderStats &renderStats = ps2_render_stats();
    renderStats.profile3DrawCalls = 0;
    renderStats.profile3Vertices = 0;
    renderStats.profile3Vu0Vertices = 0;
    renderStats.profile3Vu1Vertices = 0;
    renderStats.profile3Vu1DrawCalls = 0;
}

#endif
