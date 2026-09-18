#pragma once

struct PlatformCollisionBounds
{
    double minX;
    double minY;
    double minZ;
    double maxX;
    double maxY;
    double maxZ;
};

inline int platformCollisionStorageIndex(int localX, int localY, int localZ)
{
    return (localY << 8) | (localZ << 4) | localX;
}

inline int platformCollisionNextSectionY(int y)
{
    return ((y >> 4) + 1) << 4;
}

inline bool platformUnitCubeIntersects(const PlatformCollisionBounds &mask, int x, int y, int z)
{
    const double minX = static_cast<double>(x);
    const double minY = static_cast<double>(y);
    const double minZ = static_cast<double>(z);
    const double maxX = minX + 1.0;
    const double maxY = minY + 1.0;
    const double maxZ = minZ + 1.0;

    return mask.maxX > minX && mask.minX < maxX &&
        mask.maxY > minY && mask.minY < maxY &&
        mask.maxZ > minZ && mask.minZ < maxZ;
}

inline bool platformCanUseUnitCubeCollision(bool simpleOpaqueCube, bool specialCollisionShape)
{
    return simpleOpaqueCube && !specialCollisionShape;
}
