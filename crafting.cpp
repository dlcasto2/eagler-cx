#include "crafting.h"

#include <algorithm>

namespace {

constexpr ItemId PLANKS_A = BLOCK_PLANKS_NORMAL, PLANKS_B = BLOCK_PLANKS_DARK, PLANKS_C = BLOCK_PLANKS_BRIGHT;

constexpr Ingredient one(ItemId id, uint8_t n) { return Ingredient{ { id, 0, 0 }, n }; }
constexpr Ingredient planks(uint8_t n) { return Ingredient{ { PLANKS_A, PLANKS_B, PLANKS_C }, n }; }
constexpr Ingredient coal(uint8_t n) { return Ingredient{ { ITEM_COAL, ITEM_CHARCOAL, 0 }, n }; }
constexpr Ingredient none() { return Ingredient{ { 0, 0, 0 }, 0 }; }

constexpr Recipe simple(ItemId out, uint8_t n, Ingredient a, Ingredient b = none())
{
    return Recipe{ out, n, { a, b }, false };
}
constexpr Recipe table(ItemId out, uint8_t n, Ingredient a, Ingredient b = none())
{
    return Recipe{ out, n, { a, b }, true };
}

// The five tool shapes of one material: pickaxe and axe take 3, shovel 1,
// sword 2, plus the sticks.
#define TOOLS(mat, ingredient) \
    table(toolId(ToolMaterial::mat, ToolKind::PICKAXE), 1, ingredient(3), one(ITEM_STICK, 2)), \
    table(toolId(ToolMaterial::mat, ToolKind::AXE),     1, ingredient(3), one(ITEM_STICK, 2)), \
    table(toolId(ToolMaterial::mat, ToolKind::SHOVEL),  1, ingredient(1), one(ITEM_STICK, 2)), \
    table(toolId(ToolMaterial::mat, ToolKind::SWORD),   1, ingredient(2), one(ITEM_STICK, 1))

constexpr Ingredient cobble(uint8_t n) { return one(BLOCK_COBBLESTONE, n); }
constexpr Ingredient iron(uint8_t n) { return one(ITEM_IRON_INGOT, n); }
constexpr Ingredient gold(uint8_t n) { return one(ITEM_GOLD_INGOT, n); }
constexpr Ingredient diamond(uint8_t n) { return one(ITEM_DIAMOND, n); }

const Recipe recipes[] = {
    // Inventory (2x2) recipes
    simple(BLOCK_PLANKS_NORMAL, 4, one(BLOCK_WOOD, 1)),
    simple(ITEM_STICK, 4, planks(2)),
    simple(BLOCK_CRAFTING_TABLE, 1, planks(4)),
    simple(BLOCK_TORCH, 4, coal(1), one(ITEM_STICK, 1)),

    // Tools
    TOOLS(WOOD, planks),
    TOOLS(STONE, cobble),
    TOOLS(IRON, iron),
    TOOLS(GOLD, gold),
    TOOLS(DIAMOND, diamond),
    table(ITEM_BOW, 1, one(ITEM_STICK, 3), one(ITEM_STRING, 3)),

    // Blocks
    table(BLOCK_FURNACE, 1, cobble(8)),
    table(BLOCK_CHEST, 1, planks(8)),
    table(BLOCK_BED, 1, one(BLOCK_WOOL, 3), planks(3)),
    table(BLOCK_SMITHING_TABLE, 1, iron(2), planks(4)),
    table(BLOCK_DOOR, 3, planks(6)),
    table(BLOCK_TNT, 1, one(ITEM_GUNPOWDER, 5), one(BLOCK_SAND, 4)),
    table(ITEM_BREAD, 1, one(ITEM_WHEAT, 3)),

    // Redstone
    table(BLOCK_REDSTONE_TORCH, 1, one(ITEM_REDSTONE_DUST, 1), one(ITEM_STICK, 1)),
    table(BLOCK_REDSTONE_SWITCH, 1, one(ITEM_STICK, 1), cobble(1)),
    table(BLOCK_PRESSURE_PLATE, 1, one(BLOCK_STONE, 2)),
    table(BLOCK_REDSTONE_LAMP, 1, one(ITEM_REDSTONE_DUST, 4), one(BLOCK_GLOWSTONE, 1)),

    // Storage blocks and back
    table(BLOCK_IRON, 1, iron(9)),
    table(BLOCK_GOLD, 1, gold(9)),
    table(BLOCK_DIAMOND, 1, diamond(9)),
    table(BLOCK_EMERALD_BLOCK, 1, one(ITEM_EMERALD, 9)),
    table(ITEM_IRON_INGOT, 9, one(BLOCK_IRON, 1)),
    table(ITEM_GOLD_INGOT, 9, one(BLOCK_GOLD, 1)),
    table(ITEM_DIAMOND, 9, one(BLOCK_DIAMOND, 1)),
    table(ITEM_EMERALD, 9, one(BLOCK_EMERALD_BLOCK, 1)),

    table(ITEM_NETHERITE_INGOT, 1, one(ITEM_NETHERITE_SCRAP, 4), gold(4)),
};

#undef TOOLS

constexpr int RECIPE_COUNT = sizeof(recipes) / sizeof(recipes[0]);

void takeIngredient(PlayerInventory &inv, const Ingredient &in)
{
    unsigned left = in.count;
    for(ItemId id : in.ids)
        if(id != 0 && left > 0)
            left -= inv.remove(id, 0, left);
}

} // namespace

int recipeCount()
{
    return RECIPE_COUNT;
}

const Recipe &recipeAt(int i)
{
    return recipes[i];
}

unsigned ingredientHave(const PlayerInventory &inv, const Ingredient &in)
{
    unsigned have = 0;
    for(ItemId id : in.ids)
        if(id != 0)
            have += inv.count(id, 0);
    return have;
}

bool hasIngredients(const PlayerInventory &inv, const Recipe &r)
{
    for(const Ingredient &in : r.in)
        if(in.count > 0 && ingredientHave(inv, in) < in.count)
            return false;
    return true;
}

CraftResult craftOnce(PlayerInventory &inv, const Recipe &r)
{
    if(!hasIngredients(inv, r))
        return CraftResult::MISSING;

    // Work on a copy so a full inventory loses nothing.
    PlayerInventory after = inv;
    for(const Ingredient &in : r.in)
        if(in.count > 0)
            takeIngredient(after, in);
    if(after.add(ItemStack(r.result, r.result_count)) != 0)
        return CraftResult::NO_ROOM;

    inv = after;
    return CraftResult::OK;
}

int craftMax(PlayerInventory &inv, const Recipe &r)
{
    int made = 0;
    while(made < 64 * PlayerInventory::SLOTS && craftOnce(inv, r) == CraftResult::OK)
        ++made;
    return made;
}

int listRecipes(const PlayerInventory &inv, bool at_table, int *out, int max)
{
    int n = 0;
    for(int pass = 0; pass < 2; ++pass)
        for(int i = 0; i < RECIPE_COUNT && n < max; ++i)
        {
            const Recipe &r = recipes[i];
            if(r.needs_table && !at_table)
                continue;
            if(hasIngredients(inv, r) == (pass == 0))
                out[n++] = i;
        }
    return n;
}

const char *ingredientName(const Ingredient &in)
{
    switch(in.ids[0])
    {
    case PLANKS_A: return "Planks";
    case ITEM_COAL: return in.ids[1] == ITEM_CHARCOAL ? "Coal or Charcoal" : "Coal";
    case BLOCK_WOOD: return "Wood";
    case BLOCK_COBBLESTONE: return "Cobblestone";
    case BLOCK_STONE: return "Stone";
    case BLOCK_SAND: return "Sand";
    case BLOCK_WOOL: return "Wool";
    case BLOCK_GLOWSTONE: return "Glowstone";
    case BLOCK_IRON: return "Iron Block";
    case BLOCK_GOLD: return "Gold Block";
    case BLOCK_DIAMOND: return "Diamond Block";
    case BLOCK_EMERALD_BLOCK: return "Emerald Block";
    default:
        break;
    }
    const char *name = isBlockItem(in.ids[0]) ? nullptr : itemDef(in.ids[0]).name;
    return name ? name : "?";
}
