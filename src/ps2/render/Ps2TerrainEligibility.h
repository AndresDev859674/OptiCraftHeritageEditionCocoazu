#pragma once

#ifdef PS2_PLATFORM

enum Ps2TerrainClusterTarget
{
    PS2_TERRAIN_CLUSTER_OUTSIDE = 0,
    PS2_TERRAIN_CLUSTER_VU1 = 1,
    PS2_TERRAIN_CLUSTER_VU0 = 2
};

enum Ps2TerrainVu0Reason
{
    PS2_TERRAIN_VU0_NONE = 0,
    PS2_TERRAIN_VU0_UNAVAILABLE,
    PS2_TERRAIN_VU0_NEAR_RISK,
    PS2_TERRAIN_VU0_SIDE_RISK,
    PS2_TERRAIN_VU0_MIXED_RISK,
    PS2_TERRAIN_VU0_POLICY
};

bool ps2_terrain_cluster_uses_side_clip(int visibilityClass,
                                         int guardRisk,
                                         bool directVu1Usable);
Ps2TerrainClusterTarget ps2_terrain_cluster_target(int visibilityClass,
                                                   int guardRisk,
                                                   bool directVu1Usable);
Ps2TerrainVu0Reason ps2_terrain_vu0_reason(int visibilityClass,
                                          int guardRisk,
                                          bool directVu1Usable);

#endif
