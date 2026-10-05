#ifndef PLAYERINVENTORY_H
#define PLAYERINVENTORY_H

#include "items.h"

// The player's 36 slots: 0-8 hotbar, 9-35 storage.
class PlayerInventory
{
public:
    static constexpr int SLOTS = 36, HOTBAR = 9;

    ItemStack slots[SLOTS];
    int selected = 0;   // hotbar index

    // Tops up matching stacks first (hotbar, then storage), then fills empty
    // slots in the same order. Returns the count that didn't fit.
    unsigned add(ItemStack s);

    // Takes up to n matching items, highest slot first. Returns how many it took.
    unsigned remove(ItemId id, uint16_t meta, unsigned n);

    unsigned count(ItemId id, uint16_t meta) const;

    ItemStack &selectedStack() { return slots[selected]; }
    void clear();
};

extern PlayerInventory player_inventory;

#endif // PLAYERINVENTORY_H
