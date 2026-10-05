#include "playerinventory.h"

#include <algorithm>

constexpr int PlayerInventory::SLOTS, PlayerInventory::HOTBAR;

PlayerInventory player_inventory;

unsigned PlayerInventory::add(ItemStack s)
{
    const unsigned max_stack = itemDef(s.id).max_stack;
    unsigned left = s.count;

    if(max_stack > 1)
        for(ItemStack &slot : slots)
        {
            if(left == 0)
                break;
            if(slot.empty() || !slot.stacksWith(s) || slot.count >= max_stack)
                continue;

            const unsigned moved = std::min(left, max_stack - slot.count);
            slot.count = static_cast<uint8_t>(slot.count + moved);
            left -= moved;
        }

    for(ItemStack &slot : slots)
    {
        if(left == 0)
            break;
        if(!slot.empty())
            continue;

        const unsigned moved = std::min(left, max_stack);
        slot = ItemStack(s.id, static_cast<uint8_t>(moved), s.meta);
        left -= moved;
    }

    return left;
}

unsigned PlayerInventory::remove(ItemId id, uint16_t meta, unsigned n)
{
    unsigned taken = 0;
    for(int i = SLOTS - 1; i >= 0 && taken < n; --i)
    {
        ItemStack &slot = slots[i];
        if(slot.empty() || slot.id != id || slot.meta != meta)
            continue;

        const unsigned moved = std::min<unsigned>(n - taken, slot.count);
        slot.count = static_cast<uint8_t>(slot.count - moved);
        taken += moved;
        if(slot.count == 0)
            slot = ItemStack();
    }
    return taken;
}

unsigned PlayerInventory::count(ItemId id, uint16_t meta) const
{
    unsigned total = 0;
    for(const ItemStack &slot : slots)
        if(!slot.empty() && slot.id == id && slot.meta == meta)
            total += slot.count;
    return total;
}

void PlayerInventory::clear()
{
    for(ItemStack &slot : slots)
        slot = ItemStack();
    selected = 0;
}
