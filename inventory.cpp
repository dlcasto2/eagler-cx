#include "inventory.h"

#include "hotbar.h"
#include "playerinventory.h"

Inventory current_inventory;

constexpr int Inventory::slot_count;

void Inventory::draw(TEXTURE &tex)
{
    drawHotbar(tex);
}

unsigned int Inventory::height()
{
    return HOTBAR_H + 2;
}

BLOCK_WDATA Inventory::currentBlock() const
{
    const ItemStack &stack = player_inventory.selectedStack();
    return (stack.empty() || !isBlockItem(stack.id)) ? BLOCK_AIR : stack.toBlock();
}

void Inventory::setCurrentBlock(BLOCK_WDATA b)
{
    player_inventory.selectedStack() = ItemStack::ofBlock(b);
}

void Inventory::previousSlot()
{
    hotbarPrev();
}

void Inventory::nextSlot()
{
    hotbarNext();
}

void Inventory::resetToDefaults()
{
    static const BLOCK_WDATA defaults[slot_count] = { BLOCK_STONE, BLOCK_GRASS, BLOCK_PLANKS_NORMAL, BLOCK_TORCH, BLOCK_FLOWER,
                                                      BLOCK_COBBLESTONE, BLOCK_GLASS, BLOCK_WOOD, BLOCK_BOOKSHELF };
    player_inventory.clear();
    for(int i = 0; i < slot_count; ++i)
        player_inventory.slots[i] = ItemStack::ofBlock(defaults[i]);
}
