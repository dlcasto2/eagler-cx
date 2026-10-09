#ifndef INVENTORY_H
#define INVENTORY_H

#include "gl.h"
#include "terrain.h"

// The player's hotbar as the world sees it: the selected block, slot
// switching and drawing. The 9-slot drawing lives in hotbar.cpp.
class Inventory
{
public:
    void draw(TEXTURE &tex);

    // Height of the hotbar strip at the bottom of the screen.
    static unsigned int height();

    BLOCK_WDATA currentBlock() const;
    void setCurrentBlock(BLOCK_WDATA b);

    void previousSlot();
    void nextSlot();

    // A new world's hotbar: stone, grass, planks, torch, flower,
    // cobblestone, glass, log and bookshelf.
    void resetToDefaults();

    static constexpr int slot_count = 9;
};

extern Inventory current_inventory;

#endif // INVENTORY_H
