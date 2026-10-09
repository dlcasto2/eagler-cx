#include "screenmove.h"

namespace {

unsigned maxStack(const ItemStack &s)
{
    return itemDef(s.id).max_stack;
}

// Moves as many as fit from src onto dst (same item). Returns how many moved.
unsigned mergeInto(ItemStack &dst, ItemStack &src, unsigned limit = 255)
{
    const unsigned cap = maxStack(dst);
    if(dst.count >= cap)
        return 0;
    unsigned moving = cap - dst.count;
    if(moving > src.count) moving = src.count;
    if(moving > limit) moving = limit;
    dst.count = static_cast<uint8_t>(dst.count + moving);
    src.count = static_cast<uint8_t>(src.count - moving);
    if(src.empty())
        src = ItemStack();
    return moving;
}

} // namespace

void moveTake(ItemStack *slots, int n, int index, ItemStack &held)
{
    if(index < 0 || index >= n)
        return;
    ItemStack &slot = slots[index];

    if(held.empty())
    {
        held = slot;
        slot = ItemStack();
    }
    else if(slot.empty())
    {
        slot = held;
        held = ItemStack();
    }
    else if(slot.stacksWith(held))
        mergeInto(slot, held);
    else
    {
        const ItemStack t = slot;
        slot = held;
        held = t;
    }
}

void moveHalf(ItemStack *slots, int n, int index, ItemStack &held)
{
    if(index < 0 || index >= n || !held.empty() || slots[index].empty())
        return;
    ItemStack &slot = slots[index];
    const uint8_t take = static_cast<uint8_t>((slot.count + 1) / 2);
    held = slot;
    held.count = take;
    slot.count = static_cast<uint8_t>(slot.count - take);
    if(slot.empty())
        slot = ItemStack();
}

void movePlaceOne(ItemStack *slots, int n, int index, ItemStack &held)
{
    if(index < 0 || index >= n || held.empty())
        return;
    ItemStack &slot = slots[index];

    if(slot.empty())
    {
        slot = held;
        slot.count = 1;
        held.count = static_cast<uint8_t>(held.count - 1);
        if(held.empty())
            held = ItemStack();
    }
    else if(slot.stacksWith(held))
        mergeInto(slot, held, 1);
    else if(!(slot.id == held.id && slot.meta == held.meta))
    {
        const ItemStack t = slot;
        slot = held;
        held = t;
    }
}

void moveSendAcross(ItemStack *from, int fn, int index, ItemStack *to, int tn)
{
    if(index < 0 || index >= fn || from[index].empty())
        return;
    ItemStack &src = from[index];

    for(int i = 0; i < tn && !src.empty(); ++i)
        if(!to[i].empty() && to[i].stacksWith(src))
            mergeInto(to[i], src);

    for(int i = 0; i < tn && !src.empty(); ++i)
        if(to[i].empty())
        {
            to[i] = src;
            src = ItemStack();
        }
}

bool returnHeld(ItemStack *slots, int n, ItemStack &held)
{
    for(int i = 0; i < n && !held.empty(); ++i)
        if(!slots[i].empty() && slots[i].stacksWith(held))
            mergeInto(slots[i], held);

    for(int i = 0; i < n && !held.empty(); ++i)
        if(slots[i].empty())
        {
            slots[i] = held;
            held = ItemStack();
        }

    return held.empty();
}
