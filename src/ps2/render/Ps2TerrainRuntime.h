#pragma once

#ifdef PS2_PLATFORM

#include "platform/PlatformTuning.h"
#include "ps2/render/Ps2TerrainRenderer.h"
#include "ps2/render/Ps2TerrainCommands.h"

struct Ps2ClusterClassification
{
    unsigned char visibility[PS2_MESH_CLUSTER_COUNT];
    unsigned char guardRisk[PS2_MESH_CLUSTER_COUNT];
    bool valid;
};

struct Ps2QueuedTerrainSection
{
    Ps2RendererFrame frame;
    Ps2TerrainSectionView section;
    Ps2TerrainFallbackDraw fallback;
    bool forceVu0All;
    bool commandReady;
    bool commandFailed;
    Ps2NativeDrawContext commandContext;
    Ps2ClusterClassification classification;
};

enum Ps2OpaqueSubmitPhase
{
    PS2_OPAQUE_SUBMIT_IMMEDIATE,
    PS2_OPAQUE_SUBMIT_QUEUE,
    PS2_OPAQUE_SUBMIT_VU1,
    PS2_OPAQUE_SUBMIT_VU0
};

constexpr int PS2_MAX_QUEUED_TERRAIN_SECTIONS =
    (int)PLATFORM_MAX_RENDERED_SECTIONS_PER_PASS * 2;

struct Ps2TerrainRuntimeState
{
    Ps2TerrainClusterStats clusterStats = {};
    Ps2TerrainPass currentPass = PS2_TERRAIN_PASS_OPAQUE;
    Ps2QueuedTerrainSection queued[PS2_MAX_QUEUED_TERRAIN_SECTIONS] = {};
    int queuedCount = 0;
    Ps2OpaqueSubmitPhase opaqueSubmitPhase = PS2_OPAQUE_SUBMIT_IMMEDIATE;
    bool currentVu1Retry = false;
    bool forceVu0All = false;
    Ps2ClusterClassification* classification = nullptr;
    Ps2TerrainCommandBuffer commands;
    int traceSection = -1;
};

Ps2TerrainRuntimeState& ps2_terrain_runtime();

#endif
