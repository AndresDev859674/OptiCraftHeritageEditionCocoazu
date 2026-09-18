#include "ItemCloth.h"
#include "ItemStack.h"
#include "Block.h"
#include "BlockCloth.h"
#include "ItemDye.h"

ItemCloth::ItemCloth(int i)
    : ItemBlock(i) {
    setMaxDamage(0);
    setHasSubtypes(true);
}

int ItemCloth::getIconFromDamage(int i) {
    return Block::cloth->getBlockTextureFromSideAndMetadata(2, BlockCloth::getColorFromDamage(i));
}

int ItemCloth::getMetadata(int i) {
    return i;
}

std::string ItemCloth::getItemNameIS(ItemStack* itemstack) {
    // Java's super is ItemBlock, whose name is the block's ("tile.cloth"); going
    // straight to Item produced "item.cloth.<color>", a key no .lang file has.
    return ItemBlock::getItemName() + "." + ItemDye::dyeColors[BlockCloth::getColorFromDamage(itemstack->getItemDamage())];
}
