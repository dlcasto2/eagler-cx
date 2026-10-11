#include "doctest.h"
#include "crafting.h"

static int findRecipe(ItemId result, bool needs_table)
{
    for(int i = 0; i < recipeCount(); ++i)
        if(recipeAt(i).result == result && recipeAt(i).needs_table == needs_table)
            return i;
    return -1;
}

static const Recipe &recipeFor(ItemId result)
{
    int i = findRecipe(result, false);
    if(i < 0)
        i = findRecipe(result, true);
    REQUIRE(i >= 0);
    return recipeAt(i);
}

static unsigned total(const PlayerInventory &inv)
{
    unsigned n = 0;
    for(const ItemStack &s : inv.slots)
        n += s.count;
    return n;
}

TEST_CASE("a log makes 4 planks, 2 planks make 4 sticks") {
    PlayerInventory inv;
    inv.add(ItemStack(BLOCK_WOOD, 1));
    CHECK(craftOnce(inv, recipeFor(BLOCK_PLANKS_NORMAL)) == CraftResult::OK);
    CHECK(inv.count(BLOCK_WOOD, 0) == 0);
    CHECK(inv.count(BLOCK_PLANKS_NORMAL, 0) == 4);
    CHECK(craftOnce(inv, recipeFor(ITEM_STICK)) == CraftResult::OK);
    CHECK(inv.count(BLOCK_PLANKS_NORMAL, 0) == 2);
    CHECK(inv.count(ITEM_STICK, 0) == 4);
}

TEST_CASE("any mix of plank kinds counts as planks") {
    PlayerInventory inv;
    inv.add(ItemStack(BLOCK_PLANKS_DARK, 2));
    inv.add(ItemStack(BLOCK_PLANKS_BRIGHT, 2));
    CHECK(craftOnce(inv, recipeFor(BLOCK_CRAFTING_TABLE)) == CraftResult::OK);
    CHECK(inv.count(BLOCK_CRAFTING_TABLE, 0) == 1);
    CHECK(total(inv) == 1);
}

TEST_CASE("torches take coal or charcoal") {
    PlayerInventory inv;
    inv.add(ItemStack(ITEM_CHARCOAL, 1));
    inv.add(ItemStack(ITEM_STICK, 1));
    CHECK(craftOnce(inv, recipeFor(BLOCK_TORCH)) == CraftResult::OK);
    CHECK(inv.count(BLOCK_TORCH, 0) == 4);
    CHECK(craftOnce(inv, recipeFor(BLOCK_TORCH)) == CraftResult::MISSING);
}

TEST_CASE("missing ingredients change nothing") {
    PlayerInventory inv;
    inv.add(ItemStack(ITEM_IRON_INGOT, 2));
    inv.add(ItemStack(ITEM_STICK, 2));
    const ItemId iron_pick = toolId(ToolMaterial::IRON, ToolKind::PICKAXE);
    CHECK(craftOnce(inv, recipeFor(iron_pick)) == CraftResult::MISSING);
    CHECK(inv.count(ITEM_IRON_INGOT, 0) == 2);
    CHECK(inv.count(ITEM_STICK, 0) == 2);
}

TEST_CASE("every tool shape costs what the spec says") {
    const ToolMaterial mats[5] = { ToolMaterial::WOOD, ToolMaterial::STONE, ToolMaterial::IRON, ToolMaterial::GOLD, ToolMaterial::DIAMOND };
    const ToolKind kinds[4] = { ToolKind::PICKAXE, ToolKind::AXE, ToolKind::SHOVEL, ToolKind::SWORD };
    const int material[4] = { 3, 3, 1, 2 }, sticks[4] = { 2, 2, 2, 1 };
    for(ToolMaterial m : mats)
        for(int k = 0; k < 4; ++k)
        {
            const Recipe &r = recipeFor(toolId(m, kinds[k]));
            CHECK(r.needs_table);
            CHECK(r.result_count == 1);
            CHECK(r.in[0].count == material[k]);
            CHECK(r.in[1].ids[0] == ITEM_STICK);
            CHECK(r.in[1].count == sticks[k]);
        }
    // Netherite comes from the smithing table only.
    CHECK(findRecipe(toolId(ToolMaterial::NETHERITE, ToolKind::PICKAXE), true) < 0);
}

TEST_CASE("storage blocks go both ways") {
    PlayerInventory inv;
    inv.add(ItemStack(ITEM_EMERALD, 9));
    CHECK(craftOnce(inv, recipeFor(BLOCK_EMERALD_BLOCK)) == CraftResult::OK);
    CHECK(inv.count(BLOCK_EMERALD_BLOCK, 0) == 1);
    CHECK(craftOnce(inv, recipeFor(ITEM_EMERALD)) == CraftResult::OK);
    CHECK(inv.count(ITEM_EMERALD, 0) == 9);
    CHECK(total(inv) == 9);
}

TEST_CASE("a door recipe makes 3, netherite takes 4 scrap and 4 gold") {
    CHECK(recipeFor(BLOCK_DOOR).result_count == 3);
    PlayerInventory inv;
    inv.add(ItemStack(ITEM_NETHERITE_SCRAP, 4));
    inv.add(ItemStack(ITEM_GOLD_INGOT, 5));
    CHECK(craftOnce(inv, recipeFor(ITEM_NETHERITE_INGOT)) == CraftResult::OK);
    CHECK(inv.count(ITEM_NETHERITE_INGOT, 0) == 1);
    CHECK(inv.count(ITEM_GOLD_INGOT, 0) == 1);
}

TEST_CASE("a full inventory refuses the craft and keeps everything") {
    PlayerInventory inv;
    for(ItemStack &s : inv.slots)
        s = ItemStack(BLOCK_DIRT, 64);
    inv.slots[5] = ItemStack(ITEM_WHEAT, 3);
    CHECK(craftOnce(inv, recipeFor(ITEM_BREAD)) == CraftResult::OK);   // the wheat's slot frees up
    CHECK(inv.count(ITEM_BREAD, 0) == 1);

    inv.slots[5] = ItemStack(ITEM_WHEAT, 6);
    CHECK(craftOnce(inv, recipeFor(ITEM_BREAD)) == CraftResult::NO_ROOM);
    CHECK(inv.count(ITEM_WHEAT, 0) == 6);
}

TEST_CASE("craft max stops when the ingredients run out") {
    PlayerInventory inv;
    inv.add(ItemStack(BLOCK_WOOD, 10));
    CHECK(craftMax(inv, recipeFor(BLOCK_PLANKS_NORMAL)) == 10);
    CHECK(inv.count(BLOCK_PLANKS_NORMAL, 0) == 40);
    CHECK(craftMax(inv, recipeFor(BLOCK_PLANKS_NORMAL)) == 0);
}

TEST_CASE("crafting never creates or loses items beyond the recipe") {
    for(int i = 0; i < recipeCount(); ++i)
    {
        const Recipe &r = recipeAt(i);
        PlayerInventory inv;
        unsigned in_total = 0;
        for(const Ingredient &in : r.in)
            if(in.count > 0)
            {
                inv.add(ItemStack(in.ids[0], in.count));
                in_total += in.count;
            }
        CAPTURE(i);
        CHECK(in_total > 0);
        CHECK(craftOnce(inv, r) == CraftResult::OK);
        CHECK(total(inv) == r.result_count);
        CHECK(inv.count(r.result, 0) == r.result_count);
    }
}

TEST_CASE("the inventory lists only simple recipes, craftable first") {
    int list[MAX_RECIPES];
    CHECK(recipeCount() <= MAX_RECIPES);
    PlayerInventory inv;
    CHECK(listRecipes(inv, false, list, 64) == 4);
    CHECK(listRecipes(inv, true, list, 64) == recipeCount());

    inv.add(ItemStack(ITEM_COAL, 1));
    inv.add(ItemStack(ITEM_STICK, 1));
    const int n = listRecipes(inv, false, list, 64);
    CHECK(n == 4);
    CHECK(recipeAt(list[0]).result == BLOCK_TORCH);
    CHECK(recipeAt(list[1]).result == BLOCK_PLANKS_NORMAL);

    for(int i = 0; i < recipeCount(); ++i)
        CHECK(*ingredientName(recipeAt(i).in[0]) != '?');
}
