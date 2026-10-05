#include "items.h"

#include <cstdio>

namespace {

constexpr int TABLE_SIZE = ITEM_LAST - FIRST_ITEM + 1;

struct ToolStats { const char *material; uint8_t tier, speed; uint16_t uses; uint8_t sword, axe, pickaxe, shovel; };

// 1.8.8: ToolMaterial speed/uses; damage = ItemTool/ItemSword base + material bonus + the player's base 1.
const ToolStats tool_stats[6] = {
    { "Wooden",    1,  2,   59, 5, 4, 3, 2 },
    { "Stone",     2,  4,  131, 6, 5, 4, 3 },
    { "Iron",      3,  6,  250, 7, 6, 5, 4 },
    { "Golden",    1, 12,   32, 5, 4, 3, 2 },
    { "Diamond",   4,  8, 1561, 8, 7, 6, 5 },
    { "Netherite", 5,  9, 2031, 9, 8, 7, 6 },
};

char tool_names[24][24];

ItemDef table[TABLE_SIZE];

void material(ItemId id, const char *name)
{
    table[id - FIRST_ITEM] = ItemDef{ name, 64, ToolKind::NONE, 0, 1, 0, 1, 0, 0 };
}

void food(ItemId id, const char *name, uint8_t hunger, uint8_t saturation_x10)
{
    table[id - FIRST_ITEM] = ItemDef{ name, 64, ToolKind::NONE, 0, 1, 0, 1, hunger, saturation_x10 };
}

bool buildTable()
{
    material(ITEM_STICK, "Stick");
    material(ITEM_COAL, "Coal");
    material(ITEM_CHARCOAL, "Charcoal");
    material(ITEM_IRON_INGOT, "Iron Ingot");
    material(ITEM_GOLD_INGOT, "Gold Ingot");
    material(ITEM_DIAMOND, "Diamond");
    material(ITEM_EMERALD, "Emerald");
    material(ITEM_REDSTONE_DUST, "Redstone Dust");
    material(ITEM_NETHERITE_SCRAP, "Netherite Scrap");
    material(ITEM_NETHERITE_INGOT, "Netherite Ingot");
    material(ITEM_BONE, "Bone");
    material(ITEM_ARROW, "Arrow");
    material(ITEM_GUNPOWDER, "Gunpowder");
    material(ITEM_LEATHER, "Leather");
    material(ITEM_FEATHER, "Feather");
    material(ITEM_STRING, "String");
    material(ITEM_WHEAT, "Wheat");

    // 1.8.8 ItemFood: hunger, saturation = hunger * modifier * 2
    food(ITEM_APPLE, "Apple", 4, 24);
    food(ITEM_BREAD, "Bread", 5, 60);
    food(ITEM_PORK_RAW, "Raw Porkchop", 3, 18);
    food(ITEM_PORK_COOKED, "Cooked Porkchop", 8, 128);
    food(ITEM_BEEF_RAW, "Raw Beef", 3, 18);
    food(ITEM_BEEF_COOKED, "Steak", 8, 128);
    food(ITEM_CHICKEN_RAW, "Raw Chicken", 2, 12);
    food(ITEM_CHICKEN_COOKED, "Cooked Chicken", 6, 72);
    food(ITEM_MUTTON_RAW, "Raw Mutton", 2, 12);
    food(ITEM_MUTTON_COOKED, "Cooked Mutton", 6, 96);
    food(ITEM_ROTTEN_FLESH, "Rotten Flesh", 4, 8);

    static const char *const kind_names[4] = { "Pickaxe", "Axe", "Shovel", "Sword" };
    static const ToolKind kinds[4] = { ToolKind::PICKAXE, ToolKind::AXE, ToolKind::SHOVEL, ToolKind::SWORD };
    for(int m = 0; m < 6; ++m)
        for(int k = 0; k < 4; ++k)
        {
            const ToolStats &t = tool_stats[m];
            const uint8_t damage[4] = { t.pickaxe, t.axe, t.shovel, t.sword };
            const ItemId id = toolId(static_cast<ToolMaterial>(m), kinds[k]);
            char *name = tool_names[m * 4 + k];
            snprintf(name, sizeof(tool_names[0]), "%s %s", t.material, kind_names[k]);
            table[id - FIRST_ITEM] = ItemDef{ name, 1, kinds[k], t.tier, t.speed, t.uses, damage[k], 0, 0 };
        }

    table[ITEM_BOW - FIRST_ITEM] = ItemDef{ "Bow", 1, ToolKind::BOW, 0, 1, 384, 1, 0, 0 };
    return true;
}

const ItemDef block_item_def = { nullptr, 64, ToolKind::NONE, 0, 1, 0, 1, 0, 0 };
const ItemDef no_item_def = { nullptr, 0, ToolKind::NONE, 0, 1, 0, 1, 0, 0 };

} // namespace

const ItemDef &itemDef(ItemId id)
{
    static const bool built = buildTable();
    (void) built;

    if(isBlockItem(id))
        return block_item_def;
    if(id > ITEM_LAST)
        return no_item_def;
    return table[id - FIRST_ITEM];
}

bool isBlockItem(ItemId id)
{
    return id < FIRST_ITEM;
}

ItemStack ItemStack::ofBlock(BLOCK_WDATA b, uint8_t count)
{
    return ItemStack(getBLOCK(b), count, getBLOCKDATA(b));
}

BLOCK_WDATA ItemStack::toBlock() const
{
    return getBLOCKWDATA(static_cast<BLOCK>(id), static_cast<uint8_t>(meta));
}

bool ItemStack::stacksWith(const ItemStack &o) const
{
    return id == o.id && meta == o.meta && itemDef(id).max_stack > 1;
}
