#ifndef INVENTORY_H
#define INVENTORY_H

#include "gl.h"
#include "terrain.h"

// The creative hotbar: a 5-slot view over player_inventory slots 0-4.
// Phase 2 replaces it with the 9-slot hotbar.
class Inventory
{
public:
    void draw(TEXTURE &tex);

    static unsigned int height();

    BLOCK_WDATA currentBlock() const;
    void setCurrentBlock(BLOCK_WDATA b);

    void previousSlot();
    void nextSlot();

    // Today's defaults: stone, grass, planks, torch, flower.
    void resetToDefaults();

    static constexpr int slot_count = 5;
};

extern Inventory current_inventory;

#endif // INVENTORY_H
