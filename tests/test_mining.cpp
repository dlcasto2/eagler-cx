#include "doctest.h"
#include "mining.h"

static const ItemId WOOD_PICK = toolId(ToolMaterial::WOOD, ToolKind::PICKAXE);
static const ItemId STONE_PICK = toolId(ToolMaterial::STONE, ToolKind::PICKAXE);
static const ItemId IRON_PICK = toolId(ToolMaterial::IRON, ToolKind::PICKAXE);
static const ItemId DIAMOND_PICK = toolId(ToolMaterial::DIAMOND, ToolKind::PICKAXE);
static const ItemId WOOD_SHOVEL = toolId(ToolMaterial::WOOD, ToolKind::SHOVEL);
static const ItemId IRON_SWORD = toolId(ToolMaterial::IRON, ToolKind::SWORD);

TEST_CASE("mining time matches 1.8.8") {
    // Minecraft Wiki "Breaking": stone 7.5 s by hand, 1.15 s with a wooden pickaxe
    CHECK(miningTicks(BLOCK_STONE, 0, false, true) == 150);
    CHECK(miningTicks(BLOCK_STONE, WOOD_PICK, false, true) == 23);
    CHECK(miningTicks(BLOCK_STONE, DIAMOND_PICK, false, true) == 6);
    // dirt by hand 0.75 s, with a wooden shovel 0.4 s
    CHECK(miningTicks(BLOCK_DIRT, 0, false, true) == 15);
    CHECK(miningTicks(BLOCK_DIRT, WOOD_SHOVEL, false, true) == 8);
    // logs by hand 3 s
    CHECK(miningTicks(BLOCK_WOOD, 0, false, true) == 60);
}

TEST_CASE("water and jumping slow mining five times each") {
    const int base = miningTicks(BLOCK_DIRT, 0, false, true);
    CHECK(miningTicks(BLOCK_DIRT, 0, true, true) == base * 5);
    CHECK(miningTicks(BLOCK_DIRT, 0, false, false) == base * 5);
    CHECK(miningTicks(BLOCK_DIRT, 0, true, false) == base * 25);
}

TEST_CASE("instant and unbreakable blocks") {
    CHECK(miningTicks(BLOCK_TORCH, 0, false, true) == 0);
    CHECK(miningTicks(BLOCK_FLOWER, 0, false, true) == 0);
    CHECK(miningTicks(BLOCK_BEDROCK, DIAMOND_PICK, false, true) == MINING_NEVER);
    CHECK(miningTicks(BLOCK_WATER, 0, false, true) == MINING_NEVER);
}

TEST_CASE("swords cut cobwebs fast") {
    CHECK(miningTicks(BLOCK_SPIDERWEB, IRON_SWORD, false, true) < miningTicks(BLOCK_SPIDERWEB, 0, false, true) / 5);
}

TEST_CASE("tier gating controls drops") {
    CHECK_FALSE(canHarvest(BLOCK_IRON_ORE, WOOD_PICK));
    CHECK(canHarvest(BLOCK_IRON_ORE, STONE_PICK));
    CHECK_FALSE(canHarvest(BLOCK_DIAMOND_ORE, STONE_PICK));
    CHECK(canHarvest(BLOCK_DIAMOND_ORE, IRON_PICK));
    CHECK_FALSE(canHarvest(BLOCK_STONE, 0));
    CHECK(canHarvest(BLOCK_DIRT, 0));
    CHECK(canHarvest(BLOCK_WOOD, 0));

    uint32_t rng = 1;
    CHECK(blockDropFor(BLOCK_IRON_ORE, WOOD_PICK, rng).empty());
    const ItemStack iron = blockDropFor(BLOCK_IRON_ORE, STONE_PICK, rng);
    CHECK(iron.id == BLOCK_IRON_ORE); CHECK(iron.count == 1);
    const ItemStack cobble = blockDropFor(BLOCK_STONE, WOOD_PICK, rng);
    CHECK(cobble.id == BLOCK_COBBLESTONE);
    CHECK(blockDropFor(BLOCK_GRASS, 0, rng).id == BLOCK_DIRT);
    CHECK(blockDropFor(BLOCK_GLASS, 0, rng).empty());
    for(int i = 0; i < 50; ++i)
    {
        const ItemStack r = blockDropFor(BLOCK_REDSTONE_ORE, IRON_PICK, rng);
        CHECK(r.id == ITEM_REDSTONE_DUST); CHECK(r.count >= 4); CHECK(r.count <= 5);
    }
}

TEST_CASE("oriented blocks drop without their orientation") {
    uint32_t rng = 1;
    const ItemStack s = blockDropFor(getBLOCKWDATA(BLOCK_FURNACE, 3), STONE_PICK, rng);
    CHECK(s.id == BLOCK_FURNACE); CHECK(s.meta == 0);
}

TEST_CASE("holding mines over ticks and completes once") {
    MiningState m;
    const int needed = miningTicks(BLOCK_DIRT, 0, false, true);
    int broke = 0;
    for(int t = 0; t < needed + 3; ++t)
        if(miningTick(m, true, true, 1, 2, 3, BLOCK_DIRT, 0, false, true))
            ++broke;
    CHECK(broke == 1);
}

TEST_CASE("changing target or letting go resets progress") {
    MiningState m;
    for(int t = 0; t < 10; ++t)
        miningTick(m, true, true, 1, 2, 3, BLOCK_DIRT, 0, false, true);
    CHECK(m.progress > 0);
    miningTick(m, true, true, 1, 2, 4, BLOCK_DIRT, 0, false, true);   // another block
    CHECK(m.progress <= 1);
    for(int t = 0; t < 5; ++t)
        miningTick(m, true, true, 1, 2, 4, BLOCK_DIRT, 0, false, true);
    miningTick(m, false, true, 1, 2, 4, BLOCK_DIRT, 0, false, true);  // released
    CHECK(m.progress == 0);
    CHECK(crackStage(m) == -1);
}

TEST_CASE("unbreakable never completes") {
    MiningState m;
    for(int t = 0; t < 5000; ++t)
        CHECK_FALSE(miningTick(m, true, true, 0, 0, 0, BLOCK_BEDROCK, DIAMOND_PICK, false, true));
}

TEST_CASE("a short pause between instant breaks") {
    MiningState m;
    int broke = 0;
    for(int t = 0; t < 12; ++t)
        if(miningTick(m, true, true, t, 0, 0, BLOCK_TORCH, 0, false, true))
            ++broke;
    CHECK(broke == 2);   // 1.8.8 waits 5 ticks after a break before the next
}

TEST_CASE("crack stage runs 0 to 9") {
    MiningState m;
    const int needed = miningTicks(BLOCK_STONE, 0, false, true);
    int last = -1;
    for(int t = 0; t < needed - 1; ++t)
    {
        miningTick(m, true, true, 0, 0, 0, BLOCK_STONE, 0, false, true);
        const int s = crackStage(m);
        CHECK(s >= last); CHECK(s >= 0); CHECK(s <= 9);
        last = s;
    }
    CHECK(last == 9);
}

TEST_CASE("tool breaks at zero after its last use") {
    ItemStack pick(WOOD_PICK, 1, 0);
    for(int use = 1; use <= 58; ++use)
        wearTool(pick, toolWearFor(pick.id, BLOCK_STONE));
    CHECK_FALSE(pick.empty());
    CHECK(pick.meta == 58);
    // the 59th block still drops, then the tool breaks
    uint32_t rng = 1;
    CHECK(blockDropFor(BLOCK_STONE, pick.id, rng).id == BLOCK_COBBLESTONE);
    wearTool(pick, toolWearFor(pick.id, BLOCK_STONE));
    CHECK(pick.empty());
}

TEST_CASE("swords wear double, instant blocks and non-tools don't wear") {
    CHECK(toolWearFor(IRON_SWORD, BLOCK_DIRT) == 2);
    CHECK(toolWearFor(IRON_PICK, BLOCK_TORCH) == 0);
    CHECK(toolWearFor(ITEM_STICK, BLOCK_DIRT) == 0);
    CHECK(toolWearFor(0, BLOCK_DIRT) == 0);
}

TEST_CASE("leaves sometimes drop an apple") {
    uint32_t rng = 42; int apples = 0, other = 0;
    for(int i = 0; i < 20000; ++i)
    {
        const ItemStack s = blockDropFor(BLOCK_LEAVES, 0, rng);
        if(s.id == ITEM_APPLE) ++apples; else if(!s.empty()) ++other;
    }
    CHECK(other == 0);
    CHECK(apples > 60); CHECK(apples < 140);   // 0.5% of 20000 = 100
}
