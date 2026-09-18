#ifdef PS2_PLATFORM

#include "net/minecraft/src/RenderGlobal.h"
#include "net/minecraft/src/WorldRenderer.h"

#include "java/Arithmetic.h"
#include "net/minecraft/src/EntityLiving.h"
#include "net/minecraft/src/MathHelper.h"
#include "platform/PlatformTuning.h"
#include "platform/RenderTerrainStaging.h"
#include "platform/Log.h"
#include "ps2/render/Ps2SectionOcclusion.h"
#include "ps2/tuning/Ps2MeshMemoryPolicy.h"

#include <array>
#include <cstdint>

extern "C" long ps2_dbg_ram_free_kb();

namespace
{
    constexpr int kFaceCount = Ps2SectionOcclusion::kFaceCount;

    struct SectionVisibilityFrameScratch
    {
        std::array<std::array<std::int16_t, kFaceCount>, Ps2SectionOcclusion::kMaxSections> neighbours{};
        std::array<std::array<std::uint8_t, kFaceCount>, Ps2SectionOcclusion::kMaxSections> visibleFaces{};
        std::array<bool, Ps2SectionOcclusion::kMaxSections> visible{};
    };

    SectionVisibilityFrameScratch s_sectionVisibilityFrameScratch;
    constexpr int kOffsetX[kFaceCount] = {0, 0, 0, 0, -1, 1};
    constexpr int kOffsetY[kFaceCount] = {-1, 1, 0, 0, 0, 0};
    constexpr int kOffsetZ[kFaceCount] = {0, 0, -1, 1, 0, 0};

    int positiveModulo(int value, int modulus)
    {
        int result = value % modulus;
        if (result < 0)
            result += modulus;
        return result;
    }
}

size_t RenderGlobal::terrainMeshRamBytes() const
{
    RenderTerrainMeshRam ram;
    terrainMeshRamBreakdown(ram);
    return ram.total();
}

void RenderGlobal::terrainMeshRamBreakdown(RenderTerrainMeshRam &out) const
{
    out.stagingPool += renderTerrainStagingBytes();
    out.stagingPool += WorldRenderer::sharedOpaquePublishScratchRamBytes();

    if (worldRenderers == nullptr)
        return;

    const int_t total = renderChunksWide * renderChunksTall * renderChunksDeep;
    for (int_t i = 0; i < total; ++i)
    {
        if (worldRenderers[i] != nullptr)
            worldRenderers[i]->addTerrainMeshRam(out);
    }
}

bool RenderGlobal::trimPs2MeshCache(EntityLiving *viewer)
{
    if (worldRenderers == nullptr || viewer == nullptr)
        return false;

    const size_t beforeBytes = terrainMeshRamBytes();
    const long freeKbRaw = ps2_dbg_ram_free_kb();
    const unsigned long freeKb = freeKbRaw > 0 ? static_cast<unsigned long>(freeKbRaw) : 0u;
    const Ps2MeshMemoryPolicy::PressureLevel pressure =
        Ps2MeshMemoryPolicy::pressureLevel(beforeBytes, freeKb);
    if (pressure == Ps2MeshMemoryPolicy::PressureLevel::None)
        return false;

    const int_t total = renderChunksWide * renderChunksTall * renderChunksDeep;
    size_t trimmedBytes = 0;
    for (int_t i = 0; i < total; ++i)
    {
        WorldRenderer *renderer = worldRenderers[i];
        if (renderer == nullptr)
            continue;
        trimmedBytes += renderer->trimPs2UnusedMeshCapacity();
    }

    size_t currentBytes = beforeBytes > trimmedBytes ? beforeBytes - trimmedBytes : terrainMeshRamBytes();
    const int maxEvictions = Ps2MeshMemoryPolicy::maxEvictionsPerFrame(pressure);
    int evicted = 0;

    while (currentBytes > PS2_MESH_RAM_TARGET_BYTES && evicted < maxEvictions)
    {
        WorldRenderer *victim = nullptr;
        float farthestDistance = 0.0f;

        for (int_t i = 0; i < total; ++i)
        {
            WorldRenderer *candidate = worldRenderers[i];
            if (candidate == nullptr)
                continue;

            const float distance = candidate->distanceToEntitySquared(viewer);
            if (!Ps2MeshMemoryPolicy::canEvictRenderer(
                    candidate->hasPublishedTerrain(),
                    candidate->isTerrainBuildInProgress(),
                    candidate->isInFrustum,
                    distance))
            {
                continue;
            }

            if (victim == nullptr || distance > farthestDistance)
            {
                victim = candidate;
                farthestDistance = distance;
            }
        }

        if (victim == nullptr)
            break;

        const size_t freedBytes = victim->releasePs2PublishedMeshForBudget();
        if (freedBytes == 0)
            break;

        trimmedBytes += freedBytes;
        currentBytes = currentBytes > freedBytes ? currentBytes - freedBytes : 0;
        enqueueRendererUpdate(victim);
        ++evicted;
    }

#if MC_LOG_LEVEL >= 2
    if (trimmedBytes > 0)
    {
        MC_LOG_DEBUG("memory",
            "[PS2] mesh trim pressure=%s before=%ldKB after=%ldKB freed=%ldKB evicted=%d\n",
            pressure == Ps2MeshMemoryPolicy::PressureLevel::Hard ? "hard" : "soft",
            (long)(beforeBytes / 1024),
            (long)(currentBytes / 1024),
            (long)(trimmedBytes / 1024),
            evicted);
    }
#endif

    return true;
}

int_t RenderGlobal::ps2RendererIndexAtSection(int_t sectionX, int_t sectionY, int_t sectionZ) const
{
    if (worldRenderers == nullptr || renderChunksWide <= 0 || renderChunksTall <= 0 || renderChunksDeep <= 0)
        return -1;

    const int_t ySlot = sectionY - verticalStartSection;
    if (ySlot < 0 || ySlot >= renderChunksTall)
        return -1;

    const int_t xSlot = positiveModulo(sectionX, renderChunksWide);
    const int_t zSlot = positiveModulo(sectionZ, renderChunksDeep);
    const int_t index = (zSlot * renderChunksTall + ySlot) * renderChunksWide + xSlot;
    WorldRenderer *renderer = worldRenderers[index];
    if (renderer == nullptr ||
        JavaArithmetic::intShr(renderer->posX, 4) != sectionX ||
        JavaArithmetic::intShr(renderer->posY, 4) != sectionY ||
        JavaArithmetic::intShr(renderer->posZ, 4) != sectionZ)
    {
        return -1;
    }
    return index;
}

void RenderGlobal::updatePs2SectionVisibility(EntityLiving *viewer)
{
#if !PLATFORM_CPU_SECTION_OCCLUSION
    (void)viewer;
    return;
#else
    if (worldRenderers == nullptr)
        return;

    const int_t total = renderChunksWide * renderChunksTall * renderChunksDeep;
    if (total <= 0)
        return;

    // The PS2 renderer grid is intentionally small. If tuning ever grows it
    // beyond the fixed scratch capacity, fail open rather than hiding terrain.
    if (total > Ps2SectionOcclusion::kMaxSections || viewer == nullptr)
    {
        for (int_t i = 0; i < total; ++i)
        {
            if (worldRenderers[i] != nullptr)
                worldRenderers[i]->ps2CpuVisible = true;
        }
        return;
    }

    const int_t cameraSectionX = JavaArithmetic::intShr(MathHelper::floor_double(viewer->posX), 4);
    const int_t cameraSectionY = JavaArithmetic::intShr(MathHelper::floor_double(viewer->posY), 4);
    const int_t cameraSectionZ = JavaArithmetic::intShr(MathHelper::floor_double(viewer->posZ), 4);
    const int_t cameraIndex = ps2RendererIndexAtSection(cameraSectionX, cameraSectionY, cameraSectionZ);
    if (cameraIndex < 0)
    {
        for (int_t i = 0; i < total; ++i)
        {
            if (worldRenderers[i] != nullptr)
                worldRenderers[i]->ps2CpuVisible = true;
        }
        return;
    }

    auto &neighbours = s_sectionVisibilityFrameScratch.neighbours;
    auto &visibleFaces = s_sectionVisibilityFrameScratch.visibleFaces;
    auto &visible = s_sectionVisibilityFrameScratch.visible;
    visible.fill(false);

    for (int_t i = 0; i < total; ++i)
    {
        neighbours[static_cast<std::size_t>(i)].fill(-1);
        visibleFaces[static_cast<std::size_t>(i)].fill(Ps2SectionOcclusion::kAllFaces);

        WorldRenderer *renderer = worldRenderers[i];
        if (renderer == nullptr)
            continue;

        const int_t sectionX = JavaArithmetic::intShr(renderer->posX, 4);
        const int_t sectionY = JavaArithmetic::intShr(renderer->posY, 4);
        const int_t sectionZ = JavaArithmetic::intShr(renderer->posZ, 4);
        for (int face = 0; face < kFaceCount; ++face)
        {
            neighbours[static_cast<std::size_t>(i)][static_cast<std::size_t>(face)] =
                static_cast<std::int16_t>(ps2RendererIndexAtSection(
                    sectionX + kOffsetX[face],
                    sectionY + kOffsetY[face],
                    sectionZ + kOffsetZ[face]));
            visibleFaces[static_cast<std::size_t>(i)][static_cast<std::size_t>(face)] =
                renderer->ps2VisibleFacesFrom(face);
        }
    }

    Ps2SectionOcclusion::compute(total, cameraIndex, neighbours.data(), visibleFaces.data(), visible.data());
    for (int_t i = 0; i < total; ++i)
    {
        if (worldRenderers[i] != nullptr)
            worldRenderers[i]->ps2CpuVisible = visible[static_cast<std::size_t>(i)];
    }
#endif
}

#endif // PS2_PLATFORM

bool RenderGlobal::evictStreamingBuildForUrgent(EntityLiving *viewer)
{
    // Every staging lease is held by a streaming build. An edit next to the
    // player must not wait for one of those to finish (measured 2026-09-17:
    // 0.5-5.3 s click-to-publish with the scheduler reporting attempts=0), so
    // the least valuable one is abandoned and restarts later. Scan the whole
    // grid rather than the update queue: a lease is only ever held by a
    // renderer mid-build, wherever that renderer sits.
    if (worldRenderers == nullptr || viewer == nullptr)
        return false;

    const int_t total = renderChunksWide * renderChunksTall * renderChunksDeep;
    WorldRenderer *victim = nullptr;
    float victimDistance = 0.0f;
    for (int_t i = 0; i < total; ++i)
    {
        WorldRenderer *candidate = worldRenderers[i];
        if (candidate == nullptr || !candidate->isTerrainBuildInProgress() || candidate->urgentRebuild)
            continue;
        const float distance = candidate->distanceToEntitySquared(viewer);
        const bool better = victim == nullptr ||
            (victim->isInFrustum && !candidate->isInFrustum) ||
            (victim->isInFrustum == candidate->isInFrustum && distance > victimDistance);
        if (better)
        {
            victim = candidate;
            victimDistance = distance;
        }
    }

    if (victim == nullptr)
        return false;
    victim->abandonTerrainBuild();
    return renderTerrainStagingHasFreeSlot();
}
