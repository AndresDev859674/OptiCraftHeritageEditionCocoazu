#include "ps2/render/Ps2TerrainEligibility.h"

#ifdef PS2_PLATFORM

#include "platform/PlatformTuning.h"
#include "ps2/render/Ps2TerrainCulling.h"

bool ps2_terrain_cluster_uses_side_clip(int visibilityClass,
                                         int guardRisk,
                                         bool directVu1Usable)
{
#if PS2_VU1_SIDE_CLIPPED_PARTIALS
    return directVu1Usable && visibilityClass != 0 &&
        guardRisk == PS2_CLUSTER_GUARD_SIDE;
#else
    (void)visibilityClass;
    (void)guardRisk;
    (void)directVu1Usable;
    return false;
#endif
}

Ps2TerrainClusterTarget ps2_terrain_cluster_target(int visibilityClass,
                                                   int guardRisk,
                                                   bool directVu1Usable)
{
    if (visibilityClass == 0)
        return PS2_TERRAIN_CLUSTER_OUTSIDE;
    if (!directVu1Usable)
        return PS2_TERRAIN_CLUSTER_VU0;
    if (ps2_terrain_cluster_can_skip_clip(visibilityClass, guardRisk))
        return PS2_TERRAIN_CLUSTER_VU1;
    if (ps2_terrain_cluster_uses_side_clip(visibilityClass, guardRisk,
                                           directVu1Usable))
        return PS2_TERRAIN_CLUSTER_VU1;
#if PS2_VU1_CLIPPED_PARTIALS
    return PS2_TERRAIN_CLUSTER_VU1;
#else
    return PS2_TERRAIN_CLUSTER_VU0;
#endif
}

Ps2TerrainVu0Reason ps2_terrain_vu0_reason(int visibilityClass,
                                          int guardRisk,
                                          bool directVu1Usable)
{
    if (visibilityClass == 0 ||
        ps2_terrain_cluster_target(visibilityClass, guardRisk,
                                   directVu1Usable) != PS2_TERRAIN_CLUSTER_VU0)
    {
        return PS2_TERRAIN_VU0_NONE;
    }
    if (!directVu1Usable)
        return PS2_TERRAIN_VU0_UNAVAILABLE;

    const bool nearRisk = (guardRisk & PS2_CLUSTER_GUARD_NEAR) != 0;
    const bool sideRisk = (guardRisk & PS2_CLUSTER_GUARD_SIDE) != 0;
    if (nearRisk && sideRisk)
        return PS2_TERRAIN_VU0_MIXED_RISK;
    if (nearRisk)
        return PS2_TERRAIN_VU0_NEAR_RISK;
    if (sideRisk)
        return PS2_TERRAIN_VU0_SIDE_RISK;
    return PS2_TERRAIN_VU0_POLICY;
}

#endif
