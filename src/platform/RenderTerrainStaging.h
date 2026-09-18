#pragma once

#include <vector>
#include "java/Type.h"

// Shared scratch storage used by terrain backends while a replacement chunk
// mesh is being built. Minecraft only leases an opaque slot and receives the
// two pass buffers; the concrete pool and its memory policy belong to backend.
constexpr int RENDER_TERRAIN_STAGING_INVALID_SLOT = -1;

int renderTerrainStagingAcquire();
void renderTerrainStagingRelease(int slot);
std::vector<int_t>* renderTerrainStagingBuffers(int slot);
int renderTerrainStagingSlotsInUse();

// Whether a renderer that is not already mid build could lease a slot now.
// True on backends that keep no pool, so a scheduler guarding on this reduces
// to its previous behaviour everywhere the lease does not exist.
bool renderTerrainStagingHasFreeSlot();
