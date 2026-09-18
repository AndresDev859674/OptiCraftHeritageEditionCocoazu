#include "ps2/render/Ps2Vu1TerrainRuntime.h"

#ifdef PS2_PLATFORM

#include <algorithm>
#include <cstring>

#include "platform/Log.h"
#include "ps2/render/Ps2RenderBackend.h"
#include "ps2/render/Ps2Tuning.h"
#include "ps2/render/Ps2Vu1Path1.h"

namespace
{
    static Ps2Vu1DmaQueue s_queue;
    static Ps2Vu1TerrainCommonState s_commonState __attribute__((aligned(16)));
    static Ps2TerrainGpuState s_passGpuState;
    static Ps2Vu1TerrainStats s_stats = {};
    static long s_lifetimeVertices = 0;
    static bool s_commonStateValid = false;
    static bool s_passGpuStateValid = false;
    static int s_nextTerrainBuffer = 0;
    static int s_clippedProbeBatchesRemaining = 0;

    static_assert(PS2_VU1_CLIPPED_PROBE_BATCHES_PER_FRAME >= 0,
                  "VU1 clipped probe budget cannot be negative");
}

Ps2Vu1DmaQueue& ps2_vu1_terrain_runtime_queue()
{
    return s_queue;
}

const Ps2TerrainGpuState& ps2_vu1_terrain_runtime_gpu_state()
{
    return s_passGpuState;
}

bool ps2_vu1_terrain_runtime_pass_gpu_state_valid()
{
    return s_passGpuStateValid;
}

bool ps2_vu1_terrain_runtime_available()
{
#if !defined(PS2_ENABLE_VU1_TERRAIN) || PS2_LINEAR_DEPTH
    return false;
#else
    if (s_queue.faulted())
        ps2_vu1_path1_disable("queue");
    return !ps2_vu1_path1_disabled();
#endif
}

void ps2_vu1_terrain_runtime_begin_pass()
{
#if defined(PS2_ENABLE_VU1_TERRAIN)
    s_commonStateValid = false;
    s_nextTerrainBuffer = 0;
#if PS2_VU1_CLIPPED_PROBE_BATCHES_PER_FRAME > 0 && !PS2_VU1_CLIPPED_PARTIALS
    s_clippedProbeBatchesRemaining = PS2_VU1_CLIPPED_PROBE_BATCHES_PER_FRAME;
#else
    s_clippedProbeBatchesRemaining = 0;
#endif

    const bool prepared = ps2_render_prepare_terrain_gpu_state(s_passGpuState);
    s_passGpuStateValid = prepared &&
        !s_passGpuState.forceNearZ &&
        s_passGpuState.textureWidth == 256 &&
        s_passGpuState.textureHeight == 256 &&
        s_passGpuState.render.lightVertex == nullptr;

    static bool s_readinessReported = false;
    if (!s_readinessReported)
    {
        s_readinessReported = true;
        const char* reason =
            !ps2_vu1_terrain_runtime_available()
                ? (PS2_LINEAR_DEPTH
                    ? "off: PS2_LINEAR_DEPTH is 1"
                    : "off: disabled after a stall")
            : !PS2_DIRECT_VU1_TERRAIN
                ? "off: PS2_DIRECT_VU1_TERRAIN is 0"
            : !prepared
                ? "off: terrain GPU state unavailable"
            : s_passGpuState.forceNearZ
                ? "off: forceNearZ set on the pass"
            : s_passGpuState.textureWidth != 256 ||
              s_passGpuState.textureHeight != 256
                ? "off: terrain atlas is not 256x256"
            : s_passGpuState.render.lightVertex != nullptr
                ? "off: per-vertex lighting is active"
                : "ready";
        MC_LOG_INFO("render", "[PS2] direct VU1 terrain: %s\n", reason);
    }
#else
    s_commonStateValid = false;
    s_passGpuStateValid = false;
    s_nextTerrainBuffer = 0;
    s_clippedProbeBatchesRemaining = 0;
#endif
}

bool ps2_vu1_terrain_runtime_pass_ready()
{
    return ps2_vu1_terrain_runtime_available() && s_passGpuStateValid;
}

bool ps2_vu1_terrain_runtime_ensure_common_state(const Ps2NativeFrameContext& frame)
{
#if defined(PS2_ENABLE_VU1_TERRAIN)
    if (s_commonStateValid)
        return true;
    if (!s_passGpuStateValid)
        return false;

    ps2_vu1_terrain_build_common_state(s_commonState, frame, s_passGpuState);
    if (!ps2_vu1_terrain_append_common_state(s_queue, s_commonState))
        return false;

    s_commonStateValid = true;
    return true;
#else
    (void)frame;
    return false;
#endif
}

void ps2_vu1_terrain_runtime_invalidate_common_state()
{
    s_commonStateValid = false;
}

void ps2_vu1_terrain_runtime_invalidate_after_path_failure()
{
    s_commonStateValid = false;
    s_passGpuStateValid = false;
    s_queue.discardPending();
}

int ps2_vu1_terrain_runtime_buffer()
{
    return s_nextTerrainBuffer;
}

void ps2_vu1_terrain_runtime_advance_buffer()
{
    s_nextTerrainBuffer ^= 1;
}

bool ps2_vu1_terrain_runtime_probe_available()
{
    return s_clippedProbeBatchesRemaining > 0;
}

void ps2_vu1_terrain_runtime_consume_probe()
{
    if (s_clippedProbeBatchesRemaining > 0)
        --s_clippedProbeBatchesRemaining;
}

void ps2_vu1_terrain_runtime_disable_probes()
{
    s_clippedProbeBatchesRemaining = 0;
}

Ps2Vu1TerrainStats& ps2_vu1_terrain_runtime_stats()
{
    return s_stats;
}

void ps2_vu1_terrain_runtime_add_lifetime_vertices(int vertices)
{
    s_lifetimeVertices += vertices;
}

long ps2_vu1_terrain_runtime_lifetime_vertices()
{
    return s_lifetimeVertices;
}

void ps2_vu1_terrain_runtime_take_stats(Ps2Vu1TerrainStats& out)
{
    Ps2Vu1DmaQueueStats queueStats;
    s_queue.takeStats(queueStats);
    out = s_stats;
    out.pages += queueStats.pages;
    out.qwords += queueStats.qwords;
    out.waits += queueStats.waits;
    out.maxPageQwords = std::max(out.maxPageQwords, queueStats.maxPageQwords);
    out.waitCycles += queueStats.waitCycles;
    std::memset(&s_stats, 0, sizeof(s_stats));
}

#endif // PS2_PLATFORM
