#include "wii/render/WiiBlockRenderInfo.h"

#ifdef WII_PLATFORM

#include <array>
#include <cmath>

#include "net/minecraft/src/Block.h"
#include "net/minecraft/src/Material.h"

namespace
{
    std::array<WiiBlockRenderInfo, Block::BLOCK_REGISTRY_SIZE> s_renderInfo{};
    bool s_renderInfoReady = false;

    bool isUnitBounds(const Block *block)
    {
        if (block == nullptr)
            return false;

        constexpr float epsilon = 0.000001f;
        return std::fabs(static_cast<float>(block->minX)) <= epsilon &&
            std::fabs(static_cast<float>(block->minY)) <= epsilon &&
            std::fabs(static_cast<float>(block->minZ)) <= epsilon &&
            std::fabs(static_cast<float>(block->maxX) - 1.0f) <= epsilon &&
            std::fabs(static_cast<float>(block->maxY) - 1.0f) <= epsilon &&
            std::fabs(static_cast<float>(block->maxZ) - 1.0f) <= epsilon;
    }

    void buildRenderInfo()
    {
        if (s_renderInfoReady)
            return;

        for (int_t id = 0; id < Block::BLOCK_REGISTRY_SIZE; ++id)
        {
            WiiBlockRenderInfo info;
            Block *block = Block::blocksList[id];
            if (block != nullptr)
            {
                const int_t renderType = block->getRenderType();
                const int_t renderPass = block->getRenderBlockPass();
                const bool opaqueCube = Block::opaqueCubeLookup[id];
                const bool normalCube = block->blockMaterial != nullptr &&
                    block->blockMaterial->getIsSolid() && block->renderAsNormalBlock();
                info.renderPass = static_cast<std::uint8_t>(renderPass > 0 ? 1 : 0);
                info.simpleOpaqueCube = wiiSimpleOpaqueCubeEligible(renderType, renderPass,
                    opaqueCube, normalCube, Block::usesDefaultFaceCullingLookup[id], isUnitBounds(block));
            }
            s_renderInfo[static_cast<std::size_t>(id)] = info;
        }

        s_renderInfoReady = true;
    }
}

const WiiBlockRenderInfo &wiiGetBlockRenderInfo(int_t blockId)
{
    static const WiiBlockRenderInfo emptyInfo{};
    if (blockId < 0 || blockId >= Block::BLOCK_REGISTRY_SIZE)
        return emptyInfo;

    buildRenderInfo();
    return s_renderInfo[static_cast<std::size_t>(blockId)];
}

#else

const WiiBlockRenderInfo &wiiGetBlockRenderInfo(int_t)
{
    static const WiiBlockRenderInfo emptyInfo{};
    return emptyInfo;
}

#endif
