#include "inventory.h"

#include "texturetools.h"
#include "blockrenderer.h"
#include "playerinventory.h"

#include "textures/inventory.h"

Inventory current_inventory;

constexpr int Inventory::slot_count;

void Inventory::draw(TEXTURE &tex)
{
    drawTextureOverlay(inventory, 0, 0, tex, (SCREEN_WIDTH - inventory.width) / 2, SCREEN_HEIGHT - inventory.height - 3, inventory.width, inventory.height);
    for(unsigned int i = 0; i < slot_count; ++i)
    {
        const ItemStack &stack = player_inventory.slots[i];
        if(stack.empty() || !isBlockItem(stack.id))
            continue;

        const BLOCK_WDATA block = stack.toBlock();
        global_block_renderer.drawPreview(block, tex, (SCREEN_WIDTH - inventory.width) / 2 + 10 + i * 40, SCREEN_HEIGHT - inventory.height + (getBLOCK(block) == BLOCK_DOOR ? 2 : 6));
    }
    drawTexture(*inv_selection_p, tex, 0, 0, inv_selection_p->width, inv_selection_p->height, (SCREEN_WIDTH - inventory.width) / 2 - 1 + player_inventory.selected * 40, SCREEN_HEIGHT - inventory.height - 5, inv_selection_p->width, inv_selection_p->height);
}

unsigned int Inventory::height()
{
    return inventory.height;
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
    if(--player_inventory.selected < 0)
        player_inventory.selected = slot_count - 1;
}

void Inventory::nextSlot()
{
    if(++player_inventory.selected >= slot_count)
        player_inventory.selected = 0;
}

void Inventory::resetToDefaults()
{
    static const BLOCK_WDATA defaults[slot_count] = { BLOCK_STONE, BLOCK_GRASS, BLOCK_PLANKS_NORMAL, BLOCK_TORCH, BLOCK_FLOWER };
    player_inventory.clear();
    for(int i = 0; i < slot_count; ++i)
        player_inventory.slots[i] = ItemStack::ofBlock(defaults[i]);
}
