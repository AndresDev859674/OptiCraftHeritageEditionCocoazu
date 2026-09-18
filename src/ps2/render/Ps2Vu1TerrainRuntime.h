#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2NativeDraw.h"
#include "ps2/render/Ps2Vu1DmaQueue.h"
#include "ps2/render/Ps2Vu1Terrain.h"
#include "ps2/render/Ps2Vu1TerrainPackets.h"

Ps2Vu1DmaQueue& ps2_vu1_terrain_runtime_queue();
const Ps2TerrainGpuState& ps2_vu1_terrain_runtime_gpu_state();
bool ps2_vu1_terrain_runtime_pass_gpu_state_valid();

bool ps2_vu1_terrain_runtime_available();
void ps2_vu1_terrain_runtime_begin_pass();
bool ps2_vu1_terrain_runtime_pass_ready();
bool ps2_vu1_terrain_runtime_ensure_common_state(const Ps2NativeFrameContext& frame);

void ps2_vu1_terrain_runtime_invalidate_common_state();
void ps2_vu1_terrain_runtime_invalidate_after_path_failure();

int ps2_vu1_terrain_runtime_buffer();
void ps2_vu1_terrain_runtime_advance_buffer();

bool ps2_vu1_terrain_runtime_probe_available();
void ps2_vu1_terrain_runtime_consume_probe();
void ps2_vu1_terrain_runtime_disable_probes();

Ps2Vu1TerrainStats& ps2_vu1_terrain_runtime_stats();
void ps2_vu1_terrain_runtime_add_lifetime_vertices(int vertices);
long ps2_vu1_terrain_runtime_lifetime_vertices();
void ps2_vu1_terrain_runtime_take_stats(Ps2Vu1TerrainStats& out);

#endif // PS2_PLATFORM
