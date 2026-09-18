#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2TerrainRenderer.h"

int_t ps2_terrain_max_batch_vertices();
int_t ps2_terrain_bounded_batch_size(int_t remaining, int_t drawMode);
Ps2NativeMeshView ps2_terrain_make_raw_mesh(const Ps2TerrainSectionView& section,
                                            const int_t* raw,
                                            int_t vertexCount);
Ps2NativeMeshView ps2_terrain_make_packed_mesh(const short* positions,
                                               const short* texCoords,
                                               const unsigned char* colors,
                                               int_t firstVertex,
                                               int_t vertexCount,
                                               const Ps2NativeClampRun* clampRuns,
                                               int clampRunCount,
                                               const Ps2NativeSlice* slices = nullptr,
                                               int sliceCount = 0);

#endif
