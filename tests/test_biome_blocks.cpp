#include "doctest.h"
#include "blockdefs.h"
#include "crafting.h"
#include "furnace.h"
#include "mining.h"
#include "trees.h"

static const ItemId WOOD_SHOVEL = toolId(ToolMaterial::WOOD, ToolKind::SHOVEL);
static const ItemId WOOD_PICK = toolId(ToolMaterial::WOOD, ToolKind::PICKAXE);

static ItemStack dropOf(BLOCK_WDATA b, ItemId tool = 0)
{
    uint32_t rng = 1;
    return blockDropFor(b, tool, rng);
}

static int recipeFor(ItemId result, ItemId ingredient)
{
    for(int i = 0; i < recipeCount(); ++i)
        if(recipeAt(i).result == result && recipeAt(i).in[0].ids[0] == ingredient)
            return i;
    return -1;
}

TEST_CASE("biome blocks have 1.8.8 hardness and tools") {
    CHECK(blockDef(BLOCK_SANDSTONE).hardness_x100 == 80); CHECK(blockDef(BLOCK_SANDSTONE).min_tier == 1);
    CHECK(blockDef(BLOCK_SNOW).hardness_x100 == 20); CHECK(blockDef(BLOCK_SNOW).best_tool == ToolKind::SHOVEL);
    CHECK(blockDef(BLOCK_ICE).hardness_x100 == 50);
    CHECK(blockDef(BLOCK_CACTUS).hardness_x100 == 40);
    CHECK(blockDef(BLOCK_SPRUCE_LOG).best_tool == ToolKind::AXE); CHECK(blockDef(BLOCK_BIRCH_LOG).hardness_x100 == 200);
    CHECK(blockDef(BLOCK_SNOWY_GRASS).best_tool == ToolKind::SHOVEL);
}

TEST_CASE("what biome blocks drop") {
    CHECK(dropOf(BLOCK_SANDSTONE, WOOD_PICK).id == BLOCK_SANDSTONE);
    CHECK(dropOf(BLOCK_SANDSTONE).empty());                    // needs a pickaxe
    CHECK(dropOf(BLOCK_SNOWY_GRASS).id == BLOCK_DIRT);
    const ItemStack balls = dropOf(BLOCK_SNOW, WOOD_SHOVEL);
    CHECK(balls.id == ITEM_SNOWBALL); CHECK(balls.count == 4);
    CHECK(dropOf(BLOCK_SNOW).empty());                         // by hand: nothing
    CHECK(dropOf(BLOCK_ICE, WOOD_PICK).empty());               // ice melts instead
    CHECK(dropOf(BLOCK_CACTUS).id == BLOCK_CACTUS);
    CHECK(dropOf(BLOCK_SPRUCE_LOG).id == BLOCK_SPRUCE_LOG);
}

TEST_CASE("leaves drop their own sapling, and only oak drops apples") {
    for(BLOCK leaves : { BLOCK_LEAVES, BLOCK_SPRUCE_LEAVES, BLOCK_BIRCH_LEAVES })
    {
        const TreeKind kind = leaves == BLOCK_SPRUCE_LEAVES ? TreeKind::SPRUCE : leaves == BLOCK_BIRCH_LEAVES ? TreeKind::BIRCH : TreeKind::OAK;
        int saplings = 0, apples = 0, wrong = 0;
        uint32_t rng = 77;
        for(int i = 0; i < 20000; ++i)
        {
            const ItemStack s = blockDropFor(leaves, 0, rng);
            if(s.id == BLOCK_SAPLING) { ++saplings; if(s.meta != static_cast<uint16_t>(kind)) ++wrong; }
            if(s.id == ITEM_APPLE) ++apples;
        }
        CHECK(wrong == 0);
        CHECK(saplings > 800); CHECK(saplings < 1200);          // 1 in 20
        if(kind == TreeKind::OAK) CHECK(apples > 50); else CHECK(apples == 0);
    }
    // a sapling keeps its kind when broken
    CHECK(dropOf(getBLOCKWDATA(BLOCK_SAPLING, 2)).meta == 2);
}

TEST_CASE("dead bushes drop up to two sticks") {
    uint32_t rng = 3;
    int sticks = 0, other = 0;
    for(int i = 0; i < 3000; ++i)
    {
        const ItemStack s = blockDropFor(getBLOCKWDATA(BLOCK_FLOWER, FLOWER_DEAD_BUSH), 0, rng);
        if(s.empty()) continue;
        if(s.id == ITEM_STICK && s.count <= 2) sticks += s.count; else ++other;
    }
    CHECK(other == 0); CHECK(sticks > 2500); CHECK(sticks < 3500);   // one on average
    CHECK(dropOf(getBLOCKWDATA(BLOCK_FLOWER, 1)).id == BLOCK_FLOWER);  // flowers are still flowers
}

TEST_CASE("biome recipes") {
    REQUIRE(recipeFor(BLOCK_PLANKS_DARK, BLOCK_SPRUCE_LOG) >= 0);
    REQUIRE(recipeFor(BLOCK_PLANKS_BRIGHT, BLOCK_BIRCH_LOG) >= 0);
    CHECK(recipeAt(recipeFor(BLOCK_PLANKS_DARK, BLOCK_SPRUCE_LOG)).result_count == 4);
    REQUIRE(recipeFor(BLOCK_SANDSTONE, BLOCK_SAND) >= 0);
    REQUIRE(recipeFor(BLOCK_SNOW, ITEM_SNOWBALL) >= 0);
    CHECK_FALSE(needsTable(recipeAt(recipeFor(BLOCK_SANDSTONE, BLOCK_SAND))));   // 2x2 fits the inventory

    PlayerInventory inv;
    inv.add(ItemStack(BLOCK_SPRUCE_LOG, 1));
    CHECK(craftOnce(inv, recipeAt(recipeFor(BLOCK_PLANKS_DARK, BLOCK_SPRUCE_LOG))) == CraftResult::OK);
    CHECK(inv.count(BLOCK_PLANKS_DARK, 0) == 4);
}

TEST_CASE("every log burns and smelts into charcoal") {
    for(BLOCK log : { BLOCK_WOOD, BLOCK_SPRUCE_LOG, BLOCK_BIRCH_LOG })
    {
        CHECK(fuelTicks(ItemStack(log, 1)) == 300);
        CHECK(smeltResult(ItemStack(log, 1)).id == ITEM_CHARCOAL);
    }
    CHECK(itemDef(ITEM_SNOWBALL).max_stack == 16);
}
