#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2MeshSort.h"
#include "ps2/render/Ps2Vu0Math.h"

struct Ps2TerrainCullingContext
{
    float frustum[6][4];
    float guardBand[5][4];
    bool guardBandValid;

    // The same planes transposed for the COP2 classifier (PS2_VU0_CLUSTER_CULL).
    // Both sets are built once per frame and then read once per cluster, so the
    // transpose is paid 11 times a frame instead of the fabsf/centre/extent work
    // it removes from every cluster test.
    //
    // frustumBlock covers the six frustum planes in plane order, two padding
    // lanes in the second block. riskBlock is ordered by the risk bit a plane
    // contributes rather than by source array: lanes 0-1 are the two near
    // hazards (the homogeneous near plane frustum[4] and the safe-divide plane
    // guardBand[0]) and lanes 2-5 the four side guard planes, so the classifier
    // reads the risk mask from a constant table instead of branching per plane.
    Ps2Vu0PlaneBlock frustumBlock[2];
    Ps2Vu0PlaneBlock riskBlock[2];
};

enum Ps2TerrainClusterGuardRisk
{
    PS2_CLUSTER_GUARD_SAFE = 0,
    // Either homogeneous near (z+w) or safe-divide (w-epsilon) is not fully
    // inside. The current clipped VU1 entry must not own this geometry.
    PS2_CLUSTER_GUARD_NEAR = 1 << 0,
    PS2_CLUSTER_GUARD_SIDE = 1 << 1
};

void ps2_terrain_build_culling_context(Ps2TerrainCullingContext& out,
                                       const float* mvp,
                                       float viewW,
                                       float viewH);
int ps2_terrain_classify_cluster_visibility(const Ps2MeshCluster& cluster,
                                            const Ps2TerrainCullingContext& context);
int ps2_terrain_classify_cluster_guard_risk(const Ps2MeshCluster& cluster,
                                            const Ps2TerrainCullingContext& context);
void ps2_terrain_classify_clusters(const Ps2MeshCluster* clusters,
                                   bool sectionFullyInside,
                                   bool nativePathUsable,
                                   const Ps2TerrainCullingContext* context,
                                   int* visibility,
                                   int* guardRisk);
bool ps2_terrain_cluster_can_skip_clip(int visibilityClass, int guardRisk);

#endif
