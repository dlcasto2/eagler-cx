#include "doctest.h"
#include "furnace.h"

static StorageRecord furnace(ItemStack in, ItemStack fuel, ItemStack out = ItemStack())
{
    StorageRecord r{ StorageKind::FURNACE, std::vector<ItemStack>(FURNACE_SLOTS), 0, 0, 0 };
    r.slots[FURNACE_INPUT] = in;
    r.slots[FURNACE_FUEL] = fuel;
    r.slots[FURNACE_OUTPUT] = out;
    return r;
}

static void run(StorageRecord &r, int ticks)
{
    for(int i = 0; i < ticks; ++i)
        furnaceTick(r);
}

TEST_CASE("the spec's smelting table") {
    CHECK(smeltResult(ItemStack(BLOCK_IRON_ORE, 1)).id == ITEM_IRON_INGOT);
    CHECK(smeltResult(ItemStack(BLOCK_GOLD_ORE, 1)).id == ITEM_GOLD_INGOT);
    CHECK(smeltResult(ItemStack(BLOCK_SAND, 1)).id == BLOCK_GLASS);
    CHECK(smeltResult(ItemStack(BLOCK_COBBLESTONE, 1)).id == BLOCK_STONE);
    CHECK(smeltResult(ItemStack(BLOCK_WOOD, 1)).id == ITEM_CHARCOAL);
    CHECK(smeltResult(ItemStack(BLOCK_ANCIENT_DEBRIS, 1)).id == ITEM_NETHERITE_SCRAP);
    CHECK(smeltResult(ItemStack(ITEM_PORK_RAW, 1)).id == ITEM_PORK_COOKED);
    CHECK(smeltResult(ItemStack(ITEM_BEEF_RAW, 1)).id == ITEM_BEEF_COOKED);
    CHECK(smeltResult(ItemStack(ITEM_CHICKEN_RAW, 1)).id == ITEM_CHICKEN_COOKED);
    CHECK(smeltResult(ItemStack(ITEM_MUTTON_RAW, 1)).id == ITEM_MUTTON_COOKED);
    CHECK(smeltResult(ItemStack(BLOCK_DIRT, 1)).empty());
    CHECK(smeltResult(ItemStack(ITEM_ROTTEN_FLESH, 1)).empty());
}

TEST_CASE("fuel values") {
    CHECK(fuelTicks(ItemStack(ITEM_COAL, 1)) == 1600);
    CHECK(fuelTicks(ItemStack(ITEM_CHARCOAL, 1)) == 1600);
    CHECK(fuelTicks(ItemStack(BLOCK_WOOD, 1)) == 300);
    CHECK(fuelTicks(ItemStack(BLOCK_PLANKS_DARK, 1)) == 300);
    CHECK(fuelTicks(ItemStack(ITEM_STICK, 1)) == 100);
    CHECK(fuelTicks(ItemStack(toolId(ToolMaterial::WOOD, ToolKind::SWORD), 1)) == 200);
    CHECK(fuelTicks(ItemStack(toolId(ToolMaterial::STONE, ToolKind::PICKAXE), 1)) == 0);
    CHECK(fuelTicks(ItemStack(BLOCK_COBBLESTONE, 1)) == 0);
    CHECK(fuelTicks(ItemStack()) == 0);
}

TEST_CASE("one coal smelts exactly 8 items, each in 10 seconds") {
    StorageRecord r = furnace(ItemStack(BLOCK_IRON_ORE, 10), ItemStack(ITEM_COAL, 1));
    run(r, FURNACE_COOK_TICKS - 1);
    CHECK(r.slots[FURNACE_OUTPUT].empty());
    run(r, 1);
    CHECK(r.slots[FURNACE_OUTPUT].count == 1);
    CHECK(r.slots[FURNACE_INPUT].count == 9);
    CHECK(r.slots[FURNACE_FUEL].empty());   // the coal went in on the first tick

    run(r, 7 * FURNACE_COOK_TICKS);
    CHECK(r.slots[FURNACE_OUTPUT].count == 8);
    CHECK(r.slots[FURNACE_INPUT].count == 2);
    CHECK(furnaceBurning(r));         // lit on tick 1, so its last tick is 1601
    run(r, 1);
    CHECK_FALSE(furnaceBurning(r));
    run(r, 1000);
    CHECK(r.slots[FURNACE_OUTPUT].count == 8);   // no fuel left
    CHECK(r.cook_ticks == 0);
}

TEST_CASE("a plank smelts one and a half items") {
    StorageRecord r = furnace(ItemStack(BLOCK_SAND, 4), ItemStack(BLOCK_PLANKS_NORMAL, 1));
    run(r, 300);
    CHECK(r.slots[FURNACE_OUTPUT].count == 1);
    CHECK(r.cook_ticks == 100);       // half way through the second
    run(r, 10);
    CHECK(r.cook_ticks == 80);        // cooling off at 2 per tick
}

TEST_CASE("fuel isn't wasted when there is nothing to cook") {
    StorageRecord r = furnace(ItemStack(BLOCK_DIRT, 5), ItemStack(ITEM_COAL, 3));
    run(r, 500);
    CHECK(r.slots[FURNACE_FUEL].count == 3);
    CHECK_FALSE(furnaceBurning(r));

    StorageRecord empty = furnace(ItemStack(), ItemStack(ITEM_COAL, 3));
    run(empty, 500);
    CHECK(empty.slots[FURNACE_FUEL].count == 3);
}

TEST_CASE("a full or different output stops the furnace") {
    StorageRecord full = furnace(ItemStack(BLOCK_COBBLESTONE, 5), ItemStack(ITEM_COAL, 2), ItemStack(BLOCK_STONE, 64));
    run(full, 400);
    CHECK(full.slots[FURNACE_FUEL].count == 2);
    CHECK(full.slots[FURNACE_INPUT].count == 5);

    StorageRecord other = furnace(ItemStack(BLOCK_COBBLESTONE, 5), ItemStack(ITEM_COAL, 2), ItemStack(BLOCK_GLASS, 3));
    run(other, 400);
    CHECK(other.slots[FURNACE_FUEL].count == 2);
    CHECK(other.slots[FURNACE_OUTPUT].count == 3);
}

TEST_CASE("output stacks up and the input runs out") {
    StorageRecord r = furnace(ItemStack(ITEM_BEEF_RAW, 2), ItemStack(ITEM_COAL, 1), ItemStack(ITEM_BEEF_COOKED, 5));
    run(r, 2 * FURNACE_COOK_TICKS);
    CHECK(r.slots[FURNACE_OUTPUT].count == 7);
    CHECK(r.slots[FURNACE_INPUT].empty());
    CHECK(furnaceBurning(r));         // the coal keeps burning even with nothing left
    run(r, 1600);
    CHECK_FALSE(furnaceBurning(r));
}

TEST_CASE("fuel keeps feeding in without a gap") {
    StorageRecord r = furnace(ItemStack(BLOCK_COBBLESTONE, 64), ItemStack(ITEM_STICK, 4));
    run(r, 400);                      // four sticks = 400 ticks = 2 items
    CHECK(r.slots[FURNACE_OUTPUT].count == 2);
    CHECK(r.slots[FURNACE_FUEL].empty());
}

TEST_CASE("furnace records survive a save") {
    block_storage.clear();
    furnacePlaced(1, 2, 3);
    StorageRecord *r = block_storage.find(BlockPos{1, 2, 3});
    REQUIRE(r != nullptr);
    CHECK(r->kind == StorageKind::FURNACE);
    CHECK(r->slots.size() == FURNACE_SLOTS);
    int seen = 0;
    block_storage.forEach([&seen](const BlockPos &, StorageRecord &rec) { if(rec.kind == StorageKind::FURNACE) ++seen; });
    CHECK(seen == 1);
    block_storage.clear();
}
