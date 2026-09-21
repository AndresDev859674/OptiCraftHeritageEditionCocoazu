#include "ItemPickaxe.h"
#include "Block.h"
#include "Material.h"

#include <array>

namespace {
    // Única fuente de verdad: la lista y su tamaño viven juntos.
    // Si añades o quitas bloques, no hay ningún contador que actualizar.
    const std::array<Block*, 22>& pickaxeBlocks() {
        static const std::array<Block*, 22> blocks = {
            Block::cobblestone, Block::stairDouble, Block::stairSingle, Block::stone,
            Block::sandStone, Block::cobblestoneMossy, Block::oreIron, Block::blockSteel,
            Block::oreCoal, Block::blockGold, Block::oreGold, Block::oreDiamond,
            Block::blockDiamond, Block::ice, Block::netherrack, Block::oreLapis,
            Block::blockLapis, Block::oreRedstone, Block::oreRedstoneGlowing, Block::rail,
            Block::railDetector, Block::railPowered
        };
        return blocks;
    }
}

Block** ItemPickaxe::getBlocksEffectiveAgainst() {
    return const_cast<Block**>(pickaxeBlocks().data());
}

int ItemPickaxe::getNumBlocksEffectiveAgainst() {
    return static_cast<int>(pickaxeBlocks().size());
}

ItemPickaxe::ItemPickaxe(int id, EnumToolMaterial material)
    : ItemTool(id, 2, material,
               getBlocksEffectiveAgainst(),
               getNumBlocksEffectiveAgainst()) {
}

bool ItemPickaxe::canHarvestBlock(Block* block) {
    if (block == Block::obsidian) {
        return EnumToolMaterialHelper::getHarvestLevel(toolMaterial) == 3;
    }
    if (block == Block::blockDiamond || block == Block::oreDiamond) {
        return EnumToolMaterialHelper::getHarvestLevel(toolMaterial) >= 2;
    }
    if (block == Block::blockGold || block == Block::oreGold) {
        return EnumToolMaterialHelper::getHarvestLevel(toolMaterial) >= 2;
    }
    if (block == Block::blockSteel || block == Block::oreIron) {
        return EnumToolMaterialHelper::getHarvestLevel(toolMaterial) >= 1;
    }
    if (block == Block::blockLapis || block == Block::oreLapis) {
        return EnumToolMaterialHelper::getHarvestLevel(toolMaterial) >= 1;
    }
    if (block == Block::oreRedstone || block == Block::oreRedstoneGlowing) {
        return EnumToolMaterialHelper::getHarvestLevel(toolMaterial) >= 2;
    }
    if (block->blockMaterial == Material::rock) {
        return true;
    }
    return block->blockMaterial == Material::iron;
}

float ItemPickaxe::getStrVsBlock(ItemStack* itemstack, Block* block) {
    if (block != nullptr && (block->blockMaterial == Material::iron || block->blockMaterial == Material::rock)) {
        return efficiencyOnProperMaterial;
    }
    return ItemTool::getStrVsBlock(itemstack, block);
}