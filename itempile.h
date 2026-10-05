#ifndef ITEMPILE_H
#define ITEMPILE_H

#include <cstdint>

#include "playerinventory.h"
#include "saveio.h"

// Everything a player dropped on death (or a broken chest held), in one place.
struct ItemPile {
    GLFix x, y, z;
    uint16_t age_ticks;
    ItemStack stacks[PlayerInventory::SLOTS];
};

// Fixed slots so an entity can refer to its pile by index.
class ItemPileStore
{
public:
    static constexpr int CAPACITY = 32;

    int add(const ItemPile &pile);   // index, or -1 when full
    void remove(int index);
    ItemPile *get(int index);
    size_t size() const;
    void clear();

    bool save(SaveWriter &w) const;
    bool load(SaveReader &r);

private:
    ItemPile piles[CAPACITY];
    bool used[CAPACITY] = {};
};

extern ItemPileStore item_piles;

#endif // ITEMPILE_H
