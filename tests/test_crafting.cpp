#include "doctest.h"
#include "crafting.h"

#include <cstring>

static int findRecipe(ItemId result)
{
    for(int i = 0; i < recipeCount(); ++i)
        if(recipeAt(i).result == result)
            return i;
    return -1;
}

static const Recipe &recipeFor(ItemId result)
{
    const int i = findRecipe(result);
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
            CHECK(needsTable(r));   // every tool is three tall
            CHECK(r.result_count == 1);
            CHECK(r.in[0].count == material[k]);
            CHECK(r.in[1].ids[0] == ITEM_STICK);
            CHECK(r.in[1].count == sticks[k]);
        }
    // Netherite comes from the smithing table only.
    CHECK(findRecipe(toolId(ToolMaterial::NETHERITE, ToolKind::PICKAXE)) < 0);
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
    CHECK(listRecipes(inv, false, list, MAX_RECIPES) == 11);   // everything that fits 2x2
    CHECK(listRecipes(inv, true, list, MAX_RECIPES) == recipeCount());

    inv.add(ItemStack(ITEM_COAL, 1));
    inv.add(ItemStack(ITEM_STICK, 1));
    const int n = listRecipes(inv, false, list, MAX_RECIPES);
    CHECK(n == 11);
    CHECK(recipeAt(list[0]).result == BLOCK_TORCH);
    CHECK(recipeAt(list[1]).result == BLOCK_PLANKS_NORMAL);

    for(int i = 0; i < recipeCount(); ++i)
        CHECK(*ingredientName(recipeAt(i).in[0]) != '?');
}

// ---- the grid

static void put(ItemStack *grid, int g, const char *rows, ItemStack a, ItemStack b = ItemStack())
{
    for(int i = 0; i < g * g; ++i)
        grid[i] = rows[i] == 'a' ? a : rows[i] == 'b' ? b : ItemStack();
}

static const ItemStack PLANK(BLOCK_PLANKS_NORMAL, 1), STICK(ITEM_STICK, 1);

TEST_CASE("every shape agrees with its ingredient counts and has no empty edge") {
    for(int i = 0; i < recipeCount(); ++i)
    {
        const Recipe &r = recipeAt(i);
        CAPTURE(i);
        int na = 0, nb = 0;
        for(const char *row : r.shape)
            if(row)
                for(const char *c = row; *c; ++c)
                {
                    na += *c == 'a';
                    nb += *c == 'b';
                    CHECK((*c == 'a' || *c == 'b' || *c == ' '));
                }
        CHECK(na == r.in[0].count);
        CHECK(nb == r.in[1].count);
        if(r.shapeless)
            continue;
        int w, h;
        recipeSize(r, w, h);
        bool top = false, bottom = false, left = false, right = false;
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                const char *row = r.shape[y];
                const char c = x < static_cast<int>(strlen(row)) ? row[x] : ' ';
                if(c == ' ')
                    continue;
                top |= y == 0; bottom |= y == h - 1; left |= x == 0; right |= x == w - 1;
            }
        CHECK((top && bottom && left && right));
    }
}

TEST_CASE("each recipe's own layout matches it in the grid") {
    for(int i = 0; i < recipeCount(); ++i)
    {
        const Recipe &r = recipeAt(i);
        CAPTURE(i);
        uint8_t cells[9];
        REQUIRE(recipeLayout(r, 3, cells));
        ItemStack grid[9];
        for(int c = 0; c < 9; ++c)
            if(cells[c])
                grid[c] = ItemStack(r.in[cells[c] - 1].ids[0], 1);
        CHECK(matchGrid(grid, 3) == i);
        CHECK(recipeLayout(r, 2, cells) == !needsTable(r));
    }
}

TEST_CASE("a pickaxe needs its shape, not just its items") {
    ItemStack grid[9];
    const ItemId pick = toolId(ToolMaterial::WOOD, ToolKind::PICKAXE);
    put(grid, 3, "aaa" " b " " b ", PLANK, STICK);
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == pick);

    put(grid, 3, "aaa" "b  " "b  ", PLANK, STICK);
    CHECK(matchGrid(grid, 3) < 0);
}

TEST_CASE("shapes slide around the grid and mirror") {
    ItemStack grid[9];
    // Sticks in the right column instead of the left.
    put(grid, 3, "  a" "  a" "   ", PLANK);
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == ITEM_STICK);

    // An axe facing the other way.
    put(grid, 3, " aa" " ba" " b ", PLANK, STICK);
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == toolId(ToolMaterial::WOOD, ToolKind::AXE));
}

TEST_CASE("the 2x2 grid makes small recipes only") {
    ItemStack grid[4];
    put(grid, 2, "aaaa", PLANK);
    REQUIRE(matchGrid(grid, 2) >= 0);
    CHECK(recipeAt(matchGrid(grid, 2)).result == BLOCK_CRAFTING_TABLE);

    put(grid, 2, " a" " b", ItemStack(ITEM_CHARCOAL, 1), STICK);
    REQUIRE(matchGrid(grid, 2) >= 0);
    CHECK(recipeAt(matchGrid(grid, 2)).result == BLOCK_TORCH);

    put(grid, 2, "a   ", ItemStack(BLOCK_WOOD, 3));
    REQUIRE(matchGrid(grid, 2) >= 0);
    CHECK(recipeAt(matchGrid(grid, 2)).result == BLOCK_PLANKS_NORMAL);
}

TEST_CASE("mixed plank kinds work, a stray item breaks the match") {
    ItemStack grid[9];
    put(grid, 3, "a  " "b  " "   ", ItemStack(BLOCK_PLANKS_DARK, 1), ItemStack(BLOCK_PLANKS_BRIGHT, 1));
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == ITEM_STICK);

    grid[8] = ItemStack(BLOCK_DIRT, 1);
    CHECK(matchGrid(grid, 3) < 0);
}

TEST_CASE("netherite ingot is shapeless") {
    ItemStack grid[9];
    put(grid, 3, "abababab ", ItemStack(ITEM_NETHERITE_SCRAP, 1), ItemStack(ITEM_GOLD_INGOT, 1));
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == ITEM_NETHERITE_INGOT);
    put(grid, 3, "ababababa", ItemStack(ITEM_NETHERITE_SCRAP, 1), ItemStack(ITEM_GOLD_INGOT, 1));
    CHECK(matchGrid(grid, 3) < 0);
}

TEST_CASE("taking the result uses one item from every filled cell") {
    ItemStack grid[4];
    put(grid, 2, "aa" "aa", ItemStack(BLOCK_PLANKS_NORMAL, 3));
    consumeGrid(grid, 2);
    for(const ItemStack &s : grid)
        CHECK(s.count == 2);
    grid[0].count = 1;
    consumeGrid(grid, 2);
    CHECK(grid[0].empty());
    CHECK(grid[1].count == 1);
}

TEST_CASE("filling the grid from the inventory, one set at a time") {
    PlayerInventory inv;
    inv.add(ItemStack(BLOCK_PLANKS_NORMAL, 7));
    inv.add(ItemStack(ITEM_STICK, 5));
    ItemStack grid[9];
    const Recipe &pick = recipeFor(toolId(ToolMaterial::WOOD, ToolKind::PICKAXE));

    CHECK(fillGrid(inv, grid, 3, pick));
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == pick.result);
    CHECK(inv.count(BLOCK_PLANKS_NORMAL, 0) == 4);
    CHECK(inv.count(ITEM_STICK, 0) == 3);

    CHECK(fillGrid(inv, grid, 3, pick));            // a second set stacks on top
    CHECK(grid[0].count == 2);
    CHECK(inv.count(BLOCK_PLANKS_NORMAL, 0) == 1);
    CHECK_FALSE(fillGrid(inv, grid, 3, pick));      // short of planks: nothing moves
    CHECK(inv.count(ITEM_STICK, 0) == 1);
    CHECK(grid[0].count == 2);

    ItemStack other[9];
    other[4] = ItemStack(BLOCK_DIRT, 1);
    CHECK_FALSE(fillGrid(inv, other, 3, recipeFor(ITEM_STICK)));   // something else is in the way
    ItemStack small[4];
    CHECK_FALSE(fillGrid(inv, small, 2, pick));     // too big for 2x2
}

TEST_CASE("a bucket is three iron ingots in a V at a crafting table") {
    const Recipe &r = recipeFor(ITEM_BUCKET);
    CHECK(r.result_count == 1);
    CHECK(needsTable(r));

    const ItemStack IRON(ITEM_IRON_INGOT, 1);
    ItemStack grid[9];
    put(grid, 3, "a a" " a " "   ", IRON);
    REQUIRE(matchGrid(grid, 3) >= 0);
    CHECK(recipeAt(matchGrid(grid, 3)).result == ITEM_BUCKET);
    put(grid, 3, "   " "a a" " a ", IRON);   // lower down works too
    CHECK(recipeAt(matchGrid(grid, 3)).result == ITEM_BUCKET);
    put(grid, 3, "aaa" "   " "   ", IRON);
    CHECK(matchGrid(grid, 3) < 0);

    PlayerInventory inv;
    inv.add(ItemStack(ITEM_IRON_INGOT, 3));
    CHECK(craftOnce(inv, r) == CraftResult::OK);
    CHECK(inv.count(ITEM_BUCKET, 0) == 1);
    CHECK(inv.count(ITEM_IRON_INGOT, 0) == 0);
}
