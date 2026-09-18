#include "ps2/render/Ps2TerrainRuntime.h"

#ifdef PS2_PLATFORM

#include <cstring>

Ps2TerrainRuntimeState& ps2_terrain_runtime()
{
    static Ps2TerrainRuntimeState state;
    return state;
}

extern "C" int ps2_dbg_terrain_section()
{
    return ps2_terrain_runtime().traceSection;
}

void ps2_terrain_take_cluster_stats(Ps2TerrainClusterStats& out)
{
    Ps2TerrainRuntimeState& state = ps2_terrain_runtime();
    out = state.clusterStats;
    std::memset(&state.clusterStats, 0, sizeof(state.clusterStats));
}

#endif
