#include "furnace.h"

ItemStack smeltResult(const ItemStack &in)
{
    if(in.empty())
        return ItemStack();
    switch(in.id)
    {
    case BLOCK_IRON_ORE: return ItemStack(ITEM_IRON_INGOT, 1);
    case BLOCK_GOLD_ORE: return ItemStack(ITEM_GOLD_INGOT, 1);
    case BLOCK_SAND: return ItemStack(BLOCK_GLASS, 1);
    case BLOCK_COBBLESTONE: return ItemStack(BLOCK_STONE, 1);
    case BLOCK_WOOD: return ItemStack(ITEM_CHARCOAL, 1);
    case BLOCK_ANCIENT_DEBRIS: return ItemStack(ITEM_NETHERITE_SCRAP, 1);
    case ITEM_PORK_RAW: return ItemStack(ITEM_PORK_COOKED, 1);
    case ITEM_BEEF_RAW: return ItemStack(ITEM_BEEF_COOKED, 1);
    case ITEM_CHICKEN_RAW: return ItemStack(ITEM_CHICKEN_COOKED, 1);
    case ITEM_MUTTON_RAW: return ItemStack(ITEM_MUTTON_COOKED, 1);
    default: return ItemStack();
    }
}

uint16_t fuelTicks(const ItemStack &s)
{
    if(s.empty())
        return 0;
    switch(s.id)
    {
    case ITEM_COAL:
    case ITEM_CHARCOAL:
        return 1600;
    case BLOCK_WOOD:
    case BLOCK_PLANKS_NORMAL:
    case BLOCK_PLANKS_DARK:
    case BLOCK_PLANKS_BRIGHT:
    case BLOCK_CRAFTING_TABLE:
    case BLOCK_CHEST:
        return 300;
    case ITEM_STICK:
    case BLOCK_SAPLING:
        return 100;
    default:
        break;
    }
    if(s.id >= ITEM_FIRST_TOOL && s.id < ITEM_FIRST_TOOL + 4)   // the wooden tools
        return 200;
    return 0;
}

bool furnaceCanSmelt(const StorageRecord &r)
{
    if(r.kind != StorageKind::FURNACE || r.slots.size() < FURNACE_SLOTS)
        return false;
    const ItemStack out = smeltResult(r.slots[FURNACE_INPUT]);
    if(out.empty())
        return false;
    const ItemStack &o = r.slots[FURNACE_OUTPUT];
    if(o.empty())
        return true;
    return o.stacksWith(out) && o.count + out.count <= itemDef(o.id).max_stack;
}

void furnaceTick(StorageRecord &r)
{
    if(r.kind != StorageKind::FURNACE || r.slots.size() < FURNACE_SLOTS)
        return;

    if(r.burn_ticks_left > 0)
        --r.burn_ticks_left;

    const bool can_smelt = furnaceCanSmelt(r);

    // Light the next piece of fuel only when there's something to cook.
    if(r.burn_ticks_left == 0 && can_smelt)
    {
        ItemStack &fuel = r.slots[FURNACE_FUEL];
        const uint16_t t = fuelTicks(fuel);
        if(t > 0)
        {
            r.burn_ticks_left = r.burn_ticks_total = t;
            if(--fuel.count == 0)
                fuel = ItemStack();
        }
    }

    if(r.burn_ticks_left > 0 && can_smelt)
    {
        if(++r.cook_ticks >= FURNACE_COOK_TICKS)
        {
            r.cook_ticks = 0;
            const ItemStack out = smeltResult(r.slots[FURNACE_INPUT]);
            ItemStack &o = r.slots[FURNACE_OUTPUT];
            if(o.empty())
                o = out;
            else
                o.count = static_cast<uint8_t>(o.count + out.count);
            ItemStack &in = r.slots[FURNACE_INPUT];
            if(--in.count == 0)
                in = ItemStack();
        }
    }
    else if(r.burn_ticks_left == 0)
        r.cook_ticks = r.cook_ticks > 2 ? static_cast<uint16_t>(r.cook_ticks - 2) : 0;   // cools off
    else
        r.cook_ticks = 0;   // burning, but nothing it can cook
}

void furnacePlaced(int x, int y, int z)
{
    block_storage.create(BlockPos{x, y, z}, StorageKind::FURNACE);
}
