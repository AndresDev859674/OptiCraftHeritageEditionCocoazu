#include "platform/world/PlatformBlockCollisionInfo.h"

#if PLATFORM_FAST_BLOCK_COLLISIONS

#include <array>
#include <cmath>
#include <cstdint>

#include "net/minecraft/src/Block.h"
#include "net/minecraft/src/Material.h"
#include "platform/world/PlatformBlockCollisionMath.h"

namespace
{
    std::array<PlatformBlockCollisionInfo, Block::BLOCK_REGISTRY_SIZE> s_collisionInfo{};
    std::array<std::uint8_t, Block::BLOCK_REGISTRY_SIZE> s_collisionInfoReady{};

    bool hasUnitBounds(const Block *block)
    {
        if (block == nullptr)
            return false;

        constexpr double epsilon = 0.000001;
        return std::fabs(block->minX) <= epsilon &&
            std::fabs(block->minY) <= epsilon &&
            std::fabs(block->minZ) <= epsilon &&
            std::fabs(block->maxX - 1.0) <= epsilon &&
            std::fabs(block->maxY - 1.0) <= epsilon &&
            std::fabs(block->maxZ - 1.0) <= epsilon;
    }

    PlatformBlockCollisionInfo buildCollisionInfo(int_t blockId)
    {
        PlatformBlockCollisionInfo info;
        Block *block = Block::blocksList[blockId];
        if (block == nullptr)
            return info;

        const bool specialCollisionShape = block == Block::slowSand || block == Block::stairDouble;
        const bool simpleOpaqueCube = Block::staticOpaqueCubeLookupSafe[blockId] &&
            Block::opaqueCubeLookup[blockId] &&
            Block::usesDefaultFaceCullingLookup[blockId] &&
            block->blockMaterial != nullptr && block->blockMaterial->getIsSolid() &&
            block->renderAsNormalBlock() && block->getRenderType() == 0 &&
            block->getRenderBlockPass() == 0 && hasUnitBounds(block);

        info.unitCube = platformCanUseUnitCubeCollision(simpleOpaqueCube, specialCollisionShape);
        return info;
    }
}

const PlatformBlockCollisionInfo &platformGetBlockCollisionInfo(int_t blockId)
{
    static const PlatformBlockCollisionInfo emptyInfo{};
    if (blockId <= 0 || blockId >= Block::BLOCK_REGISTRY_SIZE)
        return emptyInfo;

    const std::size_t index = static_cast<std::size_t>(blockId);
    if (s_collisionInfoReady[index] == 0)
    {
        s_collisionInfo[index] = buildCollisionInfo(blockId);
        s_collisionInfoReady[index] = 1;
    }

    return s_collisionInfo[index];
}

#endif
