#include "platform/world/LightingDirtyRegions.h"

#include <cstdint>

#include "java/Arithmetic.h"
#include "net/minecraft/src/World.h"

std::size_t LightingDirtyRegions::slotOf(int_t sectionX, int_t sectionY, int_t sectionZ)
{
    const std::uint32_t hash = static_cast<std::uint32_t>(sectionX) * 73428767u ^
                               static_cast<std::uint32_t>(sectionZ) * 912931u ^
                               static_cast<std::uint32_t>(sectionY) * 2654435761u;
    return static_cast<std::size_t>(hash >> 16) & (SLOT_COUNT - 1);
}

void LightingDirtyRegions::begin()
{
    active = true;
}

void LightingDirtyRegions::add(World *world, int_t x, int_t y, int_t z)
{
    const int_t sectionX = JavaArithmetic::intShr(x, 4);
    const int_t sectionY = JavaArithmetic::intShr(y, 4);
    const int_t sectionZ = JavaArithmetic::intShr(z, 4);
    Region &region = regions[slotOf(sectionX, sectionY, sectionZ)];

    if (region.used &&
        (region.sectionX != sectionX || region.sectionY != sectionY || region.sectionZ != sectionZ))
    {
        flush(world, region);
    }

    if (!region.used)
    {
        region.used = true;
        region.sectionX = sectionX;
        region.sectionY = sectionY;
        region.sectionZ = sectionZ;
        region.minX = region.maxX = x;
        region.minY = region.maxY = y;
        region.minZ = region.maxZ = z;
        return;
    }

    if (x < region.minX) region.minX = x;
    if (x > region.maxX) region.maxX = x;
    if (y < region.minY) region.minY = y;
    if (y > region.maxY) region.maxY = y;
    if (z < region.minZ) region.minZ = z;
    if (z > region.maxZ) region.maxZ = z;
}

void LightingDirtyRegions::flush(World *world, Region &region)
{
    if (!region.used)
        return;
    region.used = false;
    if (world != nullptr)
        world->markBlocksDirty(region.minX, region.minY, region.minZ, region.maxX, region.maxY, region.maxZ);
}

void LightingDirtyRegions::end(World *world)
{
    active = false;
    for (Region &region : regions)
        flush(world, region);
}
