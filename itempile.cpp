#include "itempile.h"

constexpr int ItemPileStore::CAPACITY;

ItemPileStore item_piles;

int ItemPileStore::add(const ItemPile &pile)
{
    for(int i = 0; i < CAPACITY; ++i)
        if(!used[i])
        {
            used[i] = true;
            piles[i] = pile;
            return i;
        }
    return -1;
}

void ItemPileStore::remove(int index)
{
    if(index >= 0 && index < CAPACITY)
        used[index] = false;
}

ItemPile *ItemPileStore::get(int index)
{
    return (index >= 0 && index < CAPACITY && used[index]) ? &piles[index] : nullptr;
}

size_t ItemPileStore::size() const
{
    size_t n = 0;
    for(bool u : used)
        n += u;
    return n;
}

void ItemPileStore::clear()
{
    for(bool &u : used)
        u = false;
}

// u32 count; per pile: fix x, y, z; u16 age; 36 stacks.
bool ItemPileStore::save(SaveWriter &w) const
{
    w.u32(static_cast<uint32_t>(size()));
    for(int i = 0; i < CAPACITY; ++i)
    {
        if(!used[i])
            continue;
        const ItemPile &p = piles[i];
        w.fix(p.x); w.fix(p.y); w.fix(p.z); w.u16(p.age_ticks);
        for(const ItemStack &s : p.stacks)
            w.stack(s);
    }
    return w.ok();
}

bool ItemPileStore::load(SaveReader &r)
{
    clear();

    uint32_t count;
    if(!r.u32(count) || count > CAPACITY)
        return false;

    for(uint32_t i = 0; i < count; ++i)
    {
        ItemPile p{};
        if(!r.fix(p.x) || !r.fix(p.y) || !r.fix(p.z) || !r.u16(p.age_ticks))
            return false;
        for(ItemStack &s : p.stacks)
            if(!r.stack(s))
                return false;
        add(p);
    }
    return true;
}
