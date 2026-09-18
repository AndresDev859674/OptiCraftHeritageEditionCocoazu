#pragma once

class TileEntity;

inline constexpr float pcLegacyTerrainListScale()
{
    return 1.000001f;
}

inline constexpr float pcLegacyStaticTileEntityListScale()
{
    return 1.0f;
}

struct PcLegacyStaticTileEntityCandidate
{
    TileEntity *tileEntity = nullptr;
    int x = 0;
    int y = 0;
    int z = 0;
};

inline bool pcLegacyStaticTileEntityCandidateIsCurrent(
    const PcLegacyStaticTileEntityCandidate &candidate, TileEntity *current)
{
    return candidate.tileEntity != nullptr && candidate.tileEntity == current;
}

inline bool pcLegacyStaticTileEntityCandidateInsideSection(
    const PcLegacyStaticTileEntityCandidate &candidate, int sectionX, int sectionY, int sectionZ)
{
    const int localX = candidate.x - sectionX;
    const int localY = candidate.y - sectionY;
    const int localZ = candidate.z - sectionZ;
    return localX > 0 && localX < 15 &&
        localY > 0 && localY < 15 &&
        localZ > 0 && localZ < 15;
}
