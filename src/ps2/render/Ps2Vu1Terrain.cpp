#include "ps2/render/Ps2Vu1Terrain.h"

#if defined(PS2_PLATFORM) && defined(PS2_ENABLE_VU1_TERRAIN)

#include <algorithm>
#include <cstdint>

#include <gif_tags.h>
#include <gsKit.h>

#include "ps2/render/Ps2Graphics.h"
#include "ps2/render/Ps2RenderStats.h"
#include "ps2/render/Ps2Tuning.h"
#include "ps2/render/Ps2Vu1Path1.h"
#include "ps2/render/Ps2Vu1TerrainPackets.h"
#include "ps2/render/Ps2Vu1TerrainRuntime.h"

extern "C" {
extern unsigned char ps2Vu1Terrain_CodeStart[];
extern unsigned char ps2Vu1TerrainClip_Entry[];
extern unsigned char ps2Vu1TerrainCanary_Entry[];
}

namespace
{
    static const unsigned int kVifNop = 0x00;
    static const unsigned int kVifStcycl = 0x01;
    static const unsigned int kVifFlusha = 0x13;
    static const unsigned int kVifMscal = 0x14;
    static const unsigned int kVifUnpackV4_32 = 0x6c;

    static const int kCanaryDataQw = 16;
    static const int kCanaryGifQwords = 15;
    static const int kCanaryPacketQwords = kCanaryGifQwords + 2;
    static_assert(kCanaryDataQw + kCanaryGifQwords <= 1024,
                  "VU1 canary packet exceeds data memory");

    static Ps2VifQword s_canaryPacket[kCanaryPacketQwords] __attribute__((aligned(64)));

#ifdef PS2_RENDER_STATS
#define PS2_VU1_TERRAIN_STAT(expr) do { expr; } while (0)
#else
#define PS2_VU1_TERRAIN_STAT(expr) do { } while (0)
#endif

    static inline unsigned int vifCode(unsigned int command, unsigned int num,
                                       unsigned int immediate)
    {
        return (command << 24) | ((num & 0xffu) << 16) | (immediate & 0xffffu);
    }

    static inline void setPacketU64(volatile Ps2VifQword& qword, int half,
                                    unsigned long long value)
    {
        qword.w[half * 2 + 0] = (unsigned int)(value & 0xffffffffull);
        qword.w[half * 2 + 1] = (unsigned int)(value >> 32);
    }

    static inline void setPacketCommands(volatile Ps2VifQword& qword,
                                         unsigned int a, unsigned int b = 0,
                                         unsigned int c = 0, unsigned int d = 0)
    {
        qword.w[0] = a;
        qword.w[1] = b;
        qword.w[2] = c;
        qword.w[3] = d;
    }

    static bool buildCanaryPacket(volatile Ps2VifQword* packet, int microAddress,
                                  const Ps2TerrainGpuState& gpu)
    {
        if (packet == nullptr || microAddress < 0 || microAddress >= 1024 ||
            gpu.viewW <= 0.0f || gpu.viewH <= 0.0f || !gsGlobal)
            return false;

        for (int i = 0; i < kCanaryPacketQwords; ++i)
            setPacketCommands(packet[i], 0, 0, 0, 0);

        setPacketCommands(packet[0],
                          vifCode(kVifStcycl, 0, 0x0101),
                          vifCode(kVifNop, 0, 0),
                          vifCode(kVifNop, 0, 0),
                          vifCode(kVifUnpackV4_32, kCanaryGifQwords,
                                  kCanaryDataQw));

        const unsigned long long adTag = GIF_TAG(2, 0, 0, 0, 0, 1);
        setPacketU64(packet[1], 0, adTag);
        setPacketU64(packet[1], 1, GIF_AD);
        setPacketU64(packet[2], 0, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
        setPacketU64(packet[2], 1, (unsigned long long)(GS_TEST_1 + gpu.primContext));
        setPacketU64(packet[3], 0,
                     GS_SETREG_ZBUF(gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, 1));
        setPacketU64(packet[3], 1, (unsigned long long)(GS_ZBUF_1 + gpu.primContext));

        const unsigned long long prim = GS_SETREG_PRIM(
            GS_PRIM_PRIM_TRISTRIP, 0, 0, 0, 0, 0, 0, gpu.primContext, 0);
        const unsigned long long drawTag = GIF_TAG(4, 0, 1, prim, 0, 2);
        setPacketU64(packet[4], 0, drawTag);
        setPacketU64(packet[4], 1,
                     (unsigned long long)GIF_REG_RGBAQ |
                     ((unsigned long long)GIF_REG_XYZ2 << 4));

        const int halfWidth = std::min(80, (int)gpu.viewW / 4);
        const int halfHeight = std::min(48, (int)gpu.viewH / 4);
        const int centerX = gpu.offsetX + (int)(gpu.viewW * 8.0f);
        const int centerY = gpu.offsetY + (int)(gpu.viewH * 8.0f);
        const unsigned int x0 = (unsigned int)(centerX - halfWidth * 16);
        const unsigned int x1 = (unsigned int)(centerX + halfWidth * 16);
        const unsigned int y0 = (unsigned int)(centerY - halfHeight * 16);
        const unsigned int y1 = (unsigned int)(centerY + halfHeight * 16);
        const unsigned long long magenta = GS_SETREG_RGBAQ(
            0x80, 0x00, 0x80, 0x80, 0x3f800000);
        const unsigned long long vertices[4] = {
            GS_SETREG_XYZ2(x0, y0, 0),
            GS_SETREG_XYZ2(x0, y1, 0),
            GS_SETREG_XYZ2(x1, y0, 0),
            GS_SETREG_XYZ2(x1, y1, 0)
        };
        for (int vertex = 0; vertex < 4; ++vertex)
        {
            setPacketU64(packet[5 + vertex * 2], 0, magenta);
            setPacketU64(packet[6 + vertex * 2], 0, vertices[vertex]);
        }

        const unsigned long long restoreTag = GIF_TAG(2, 1, 0, 0, 0, 1);
        setPacketU64(packet[13], 0, restoreTag);
        setPacketU64(packet[13], 1, GIF_AD);
        setPacketU64(packet[14], 0, gpu.test);
        setPacketU64(packet[14], 1, (unsigned long long)(GS_TEST_1 + gpu.primContext));
        setPacketU64(packet[15], 0, gpu.zbuf);
        setPacketU64(packet[15], 1, (unsigned long long)(GS_ZBUF_1 + gpu.primContext));

        setPacketCommands(packet[16],
                          vifCode(kVifMscal, 0, (unsigned int)microAddress),
                          vifCode(kVifFlusha, 0, 0),
                          vifCode(kVifNop, 0, 0),
                          vifCode(kVifNop, 0, 0));
        return true;
    }
}

bool ps2_vu1_terrain_available()
{
    return ps2_vu1_terrain_runtime_available();
}

void ps2_vu1_terrain_begin_pass()
{
    ps2_vu1_terrain_runtime_begin_pass();
}

bool ps2_vu1_terrain_pass_ready()
{
    return ps2_vu1_terrain_runtime_pass_ready();
}

bool ps2_vu1_terrain_clipped_probe_available()
{
    return ps2_vu1_terrain_runtime_probe_available();
}

bool ps2_vu1_terrain_draw_canary()
{
#if PS2_VU1_TERRAIN_CANARY
    Ps2Vu1TerrainStats& stats = ps2_vu1_terrain_runtime_stats();
    const Ps2TerrainGpuState& gpu = ps2_vu1_terrain_runtime_gpu_state();
    const int entryBytes = (int)(ps2Vu1TerrainCanary_Entry - ps2Vu1Terrain_CodeStart);
    if (!ps2_vu1_terrain_runtime_pass_gpu_state_valid() ||
        entryBytes < 0 || (entryBytes & 7) != 0 ||
        !ps2_render_acquire_path1())
    {
        PS2_VU1_TERRAIN_STAT(++stats.canaryFailures);
        return false;
    }

    volatile Ps2VifQword* packet = reinterpret_cast<volatile Ps2VifQword*>(
        reinterpret_cast<std::uintptr_t>(s_canaryPacket) | 0x30000000u);
    if (!buildCanaryPacket(packet, entryBytes / 8, gpu) ||
        !ps2_vu1_path1_send_flusha())
    {
        PS2_VU1_TERRAIN_STAT(++stats.canaryFailures);
        return false;
    }

    PS2_VU1_TERRAIN_STAT(++stats.canarySubmitted);
    if (!ps2_vu1_path1_send_normal_and_wait(
            const_cast<Ps2VifQword*>(packet), kCanaryPacketQwords, "canary"))
    {
        PS2_VU1_TERRAIN_STAT(++stats.canaryFailures);
        return false;
    }

    PS2_VU1_TERRAIN_STAT(++stats.canaryCompleted);
    return true;
#else
    return false;
#endif
}

Ps2Vu1TerrainDrawResult ps2_vu1_terrain_draw_slices(const Ps2TerrainMesh& mesh,
                                                    const Ps2Vu1TerrainSlice* slices,
                                                    int sliceCount,
                                                    int totalVertices,
                                                    int tileX, int tileY,
                                                    const Ps2NativeFrameContext& frame,
                                                    float translateX,
                                                    float translateY,
                                                    float translateZ)
{
    Ps2Vu1TerrainDrawResult result = { PS2_VU1_TERRAIN_RETRY_NATIVE, 0 };
    if (!ps2_vu1_terrain_available() || !mesh.valid() || !frame.valid ||
        slices == nullptr || sliceCount <= 0 || totalVertices <= 0 ||
        totalVertices > PS2_VU1_TERRAIN_MAX_VERTICES || (totalVertices & 3) != 0)
        return result;
    if (!ps2_vu1_terrain_runtime_pass_gpu_state_valid())
        return result;
    if (!ps2_render_acquire_path1() ||
        !ps2_vu1_terrain_runtime_ensure_common_state(frame))
        return result;

    Ps2Vu1DmaQueue& queue = ps2_vu1_terrain_runtime_queue();
    const Ps2TerrainGpuState& gpu = ps2_vu1_terrain_runtime_gpu_state();
    const int terrainBuffer = ps2_vu1_terrain_runtime_buffer();
    if (!ps2_vu1_terrain_append_sliced_batch(
            queue, mesh, slices, sliceCount, totalVertices,
            translateX, translateY, translateZ, gpu,
            tileX, tileY, terrainBuffer, 0))
        return result;

    Ps2Vu1TerrainStats& stats = ps2_vu1_terrain_runtime_stats();
    PS2_VU1_TERRAIN_STAT(
        terrainBuffer == 0 ? ++stats.buffer0Batches : ++stats.buffer1Batches);
    ps2_vu1_terrain_runtime_advance_buffer();
    PS2_VU1_TERRAIN_STAT(++stats.batches);
    PS2_VU1_TERRAIN_STAT(stats.vertices += totalVertices);
#if MC_LOG_LEVEL > 2
    ps2_render_stats().profile3Vu1Vertices += totalVertices;
    ++ps2_render_stats().profile3Vu1DrawCalls;
#endif
    PS2_VU1_TERRAIN_STAT(stats.qwords += totalVertices);
    PS2_VU1_TERRAIN_STAT(++stats.xgkicks);
    ps2_vu1_terrain_runtime_add_lifetime_vertices(totalVertices);
    result.vertices = totalVertices;
    result.status = PS2_VU1_TERRAIN_SUBMITTED;
    return result;
}

Ps2Vu1TerrainDrawResult ps2_vu1_terrain_draw_range(const Ps2TerrainMesh& mesh,
                                                   int firstVertex,
                                                   int vertexCount,
                                                   const Ps2NativeFrameContext& frame,
                                                   float translateX,
                                                   float translateY,
                                                   float translateZ,
                                                   bool fullyInside)
{
    Ps2Vu1TerrainDrawResult result = { PS2_VU1_TERRAIN_RETRY_NATIVE, 0 };
    if (!ps2_vu1_terrain_available() || !mesh.valid() || !frame.valid ||
        firstVertex < 0 || vertexCount <= 0 ||
        (firstVertex & 3) != 0 || (vertexCount & 3) != 0 ||
        firstVertex + vertexCount > mesh.vertexCount())
        return result;
#if !PS2_VU1_CLIPPED_PARTIALS && !PS2_VU1_SIDE_CLIPPED_PARTIALS && PS2_VU1_CLIPPED_PROBE_BATCHES_PER_FRAME <= 0
    if (!fullyInside)
        return result;
#endif

    if (!ps2_vu1_terrain_runtime_pass_gpu_state_valid())
        return result;
    if (!ps2_render_acquire_path1() ||
        !ps2_vu1_terrain_runtime_ensure_common_state(frame))
        return result;

    const bool clipped = !fullyInside;
    const int entryBytes = clipped
        ? (int)(ps2Vu1TerrainClip_Entry - ps2Vu1Terrain_CodeStart)
        : 0;
    if (entryBytes < 0 || (entryBytes & 7) != 0)
        return result;
    const int microAddress = entryBytes / 8;
    const int maxBatchVertices = clipped
        ? PS2_VU1_TERRAIN_CLIPPED_MAX_VERTICES
        : PS2_VU1_TERRAIN_MAX_VERTICES;

    Ps2Vu1DmaQueue& queue = ps2_vu1_terrain_runtime_queue();
    const Ps2TerrainGpuState& gpu = ps2_vu1_terrain_runtime_gpu_state();
    Ps2Vu1TerrainStats& stats = ps2_vu1_terrain_runtime_stats();
    const int rangeEnd = firstVertex + vertexCount;
    const std::vector<Ps2TerrainTileRun>& runs = mesh.runs();
    for (std::size_t i = 0; i < runs.size(); ++i)
    {
        const Ps2TerrainTileRun& run = runs[i];
        const int runBegin = std::max(firstVertex, (int)run.firstVertex);
        const int runEnd = std::min(rangeEnd, (int)(run.firstVertex + run.vertexCount));
        if (runBegin >= runEnd)
            continue;

        int cursor = runBegin;
        while (cursor < runEnd)
        {
            int batch = std::min(maxBatchVertices, runEnd - cursor);
            batch -= batch & 3;
            if (batch <= 0)
                break;

            const int terrainBuffer = ps2_vu1_terrain_runtime_buffer();
            if (!ps2_vu1_terrain_append_batch(
                    queue, mesh, cursor, batch,
                    translateX, translateY, translateZ,
                    gpu, run.tileX, run.tileY,
                    terrainBuffer, microAddress, clipped))
            {
                result.status = result.vertices > 0
                    ? PS2_VU1_TERRAIN_FATAL
                    : PS2_VU1_TERRAIN_RETRY_NATIVE;
                return result;
            }
            PS2_VU1_TERRAIN_STAT(
                terrainBuffer == 0
                    ? ++stats.buffer0Batches
                    : ++stats.buffer1Batches);
            ps2_vu1_terrain_runtime_advance_buffer();
            PS2_VU1_TERRAIN_STAT(++stats.batches);
            PS2_VU1_TERRAIN_STAT(stats.vertices += batch);
#if MC_LOG_LEVEL > 2
            ps2_render_stats().profile3Vu1Vertices += batch;
            ++ps2_render_stats().profile3Vu1DrawCalls;
#endif
            PS2_VU1_TERRAIN_STAT(if (clipped) ++stats.clippedBatches);
            PS2_VU1_TERRAIN_STAT(if (clipped) stats.clippedVertices += batch);
            PS2_VU1_TERRAIN_STAT(stats.qwords += batch);
            PS2_VU1_TERRAIN_STAT(++stats.xgkicks);
            cursor += batch;
            result.vertices += batch;
            ps2_vu1_terrain_runtime_add_lifetime_vertices(batch);
        }
    }

    if (result.vertices == vertexCount)
        result.status = PS2_VU1_TERRAIN_SUBMITTED;
    else if (result.vertices > 0)
        result.status = PS2_VU1_TERRAIN_FATAL;
    return result;
}

Ps2Vu1TerrainDrawResult ps2_vu1_terrain_probe_clipped_range(
                                                   const Ps2TerrainMesh& mesh,
                                                   int firstVertex,
                                                   int vertexCount,
                                                   const Ps2NativeFrameContext& frame,
                                                   float translateX,
                                                   float translateY,
                                                   float translateZ)
{
    Ps2Vu1TerrainDrawResult result = { PS2_VU1_TERRAIN_RETRY_NATIVE, 0 };
#if PS2_VU1_CLIPPED_PROBE_BATCHES_PER_FRAME > 0 && !PS2_VU1_CLIPPED_PARTIALS
    if (!ps2_vu1_terrain_runtime_probe_available() || !mesh.valid() ||
        firstVertex < 0 || vertexCount <= 0 ||
        (firstVertex & 3) != 0 || (vertexCount & 3) != 0 ||
        firstVertex + vertexCount > mesh.vertexCount())
        return result;

    Ps2Vu1TerrainStats& stats = ps2_vu1_terrain_runtime_stats();
    const int rangeEnd = firstVertex + vertexCount;
    const std::vector<Ps2TerrainTileRun>& runs = mesh.runs();
    for (std::size_t i = 0; i < runs.size(); ++i)
    {
        const Ps2TerrainTileRun& run = runs[i];
        const int runBegin = std::max(firstVertex, (int)run.firstVertex);
        const int runEnd = std::min(rangeEnd,
            (int)(run.firstVertex + run.vertexCount));
        int batch = std::min(PS2_VU1_TERRAIN_CLIPPED_MAX_VERTICES,
            runEnd - runBegin);
        batch -= batch & 3;
        if (batch <= 0)
            continue;

        ps2_vu1_terrain_runtime_consume_probe();
        result = ps2_vu1_terrain_draw_range(mesh, runBegin, batch, frame,
            translateX, translateY, translateZ, false);
        if (result.status == PS2_VU1_TERRAIN_SUBMITTED)
        {
            PS2_VU1_TERRAIN_STAT(++stats.clippedProbeBatches);
            PS2_VU1_TERRAIN_STAT(stats.clippedProbeVertices += result.vertices);
        }
        else
        {
            PS2_VU1_TERRAIN_STAT(++stats.clippedProbeRetries);
        }
        return result;
    }

    ps2_vu1_terrain_runtime_disable_probes();
    PS2_VU1_TERRAIN_STAT(++stats.clippedProbeRetries);
#else
    (void)mesh;
    (void)firstVertex;
    (void)vertexCount;
    (void)frame;
    (void)translateX;
    (void)translateY;
    (void)translateZ;
#endif
    return result;
}

long ps2_vu1_terrain_submitted_vertices()
{
    return ps2_vu1_terrain_runtime_lifetime_vertices();
}

void ps2_vu1_terrain_take_stats(Ps2Vu1TerrainStats& out)
{
    ps2_vu1_terrain_runtime_take_stats(out);
}

#else

#if defined(PS2_PLATFORM)

#include <cstring>

bool ps2_vu1_terrain_available() { return false; }
void ps2_vu1_terrain_begin_pass() {}
bool ps2_vu1_terrain_pass_ready() { return false; }
bool ps2_vu1_terrain_draw_canary() { return false; }
bool ps2_vu1_terrain_clipped_probe_available() { return false; }

Ps2Vu1TerrainDrawResult ps2_vu1_terrain_draw_slices(const Ps2TerrainMesh&,
                                                    const Ps2Vu1TerrainSlice*, int, int,
                                                    int, int, const Ps2NativeFrameContext&,
                                                    float, float, float)
{
    Ps2Vu1TerrainDrawResult result = { PS2_VU1_TERRAIN_RETRY_NATIVE, 0 };
    return result;
}

Ps2Vu1TerrainDrawResult ps2_vu1_terrain_draw_range(const Ps2TerrainMesh&,
                                                   int, int,
                                                   const Ps2NativeFrameContext&,
                                                   float, float, float, bool)
{
    Ps2Vu1TerrainDrawResult result = { PS2_VU1_TERRAIN_RETRY_NATIVE, 0 };
    return result;
}

Ps2Vu1TerrainDrawResult ps2_vu1_terrain_probe_clipped_range(
                                                   const Ps2TerrainMesh&,
                                                   int, int,
                                                   const Ps2NativeFrameContext&,
                                                   float, float, float)
{
    Ps2Vu1TerrainDrawResult result = { PS2_VU1_TERRAIN_RETRY_NATIVE, 0 };
    return result;
}

long ps2_vu1_terrain_submitted_vertices() { return 0; }

void ps2_vu1_terrain_take_stats(Ps2Vu1TerrainStats& out)
{
    std::memset(&out, 0, sizeof(out));
}

#endif
#endif
