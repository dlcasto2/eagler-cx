#include "doctest.h"
#include "containers.h"
#include "drops.h"
#include "gamestate.h"

static const ItemId DIAMOND_PICK = toolId(ToolMaterial::DIAMOND, ToolKind::PICKAXE);
static const ItemId NETHERITE_PICK = toolId(ToolMaterial::NETHERITE, ToolKind::PICKAXE);
static const ItemId IRON_SWORD = toolId(ToolMaterial::IRON, ToolKind::SWORD);

static AABB playerAt(GLFix x, GLFix y, GLFix z)
{
    return AABB(x - 51, y, z - 51, x + 51, y + 230, z + 51);
}

TEST_CASE("a placed chest gets 27 empty slots, breaking it gives them back") {
    block_storage.clear();
    chestPlaced(4, 10, -2);
    StorageRecord *r = block_storage.find(BlockPos{4, 10, -2});
    REQUIRE(r != nullptr);
    CHECK(r->kind == StorageKind::CHEST);
    CHECK(r->slots.size() == 27);
    r->slots[3] = ItemStack(ITEM_COAL, 12);
    r->slots[26] = ItemStack(BLOCK_DIRT, 64);
    const std::vector<ItemStack> out = chestBroken(4, 10, -2);
    CHECK(out.size() == 2);
    CHECK(block_storage.find(BlockPos{4, 10, -2}) == nullptr);
    CHECK(chestBroken(4, 10, -2).empty());   // nothing there any more
}

TEST_CASE("a pile holds everything and is collected by walking over it") {
    EntityPool pool; ItemPileStore piles; PlayerInventory inv;
    const ItemStack stuff[3] = { ItemStack(ITEM_COAL, 12), ItemStack(BLOCK_DIRT, 64), ItemStack(IRON_SWORD, 1, 30) };
    CHECK(dropPile(pool, piles, 640, 1280, 640, stuff, 3, inv));
    CHECK(piles.size() == 1);
    int pile_entities = 0;
    pool.forEach([&pile_entities](Entity &e) { if(e.type == EntityType::ITEM_PILE) ++pile_entities; });
    CHECK(pile_entities == 1);

    collectPiles(pool, piles, playerAt(640, 1280, 640), inv);
    CHECK(piles.size() == 1);            // too fresh: the same half-second delay as drops
    pool.forEach([](Entity &e) { e.age_ticks = PICKUP_DELAY_TICKS; });
    collectPiles(pool, piles, playerAt(640 + 128 * 5, 1280, 640), inv);
    CHECK(piles.size() == 1);            // too far
    collectPiles(pool, piles, playerAt(640, 1280, 640), inv);
    CHECK(piles.size() == 0);
    CHECK(pool.count() == 0);
    CHECK(inv.count(ITEM_COAL, 0) == 12);
    CHECK(inv.count(BLOCK_DIRT, 0) == 64);
    CHECK(inv.count(IRON_SWORD, 30) == 1);
}

TEST_CASE("a pile you can't fully carry keeps the rest") {
    EntityPool pool; ItemPileStore piles; PlayerInventory inv;
    for(auto &s : inv.slots) s = ItemStack(ITEM_STICK, 64);
    inv.slots[0] = ItemStack(ITEM_COAL, 60);
    const ItemStack stuff[2] = { ItemStack(ITEM_COAL, 10), ItemStack(BLOCK_SAND, 5) };
    dropPile(pool, piles, 0, 0, 0, stuff, 2, inv);
    pool.forEach([](Entity &e) { e.age_ticks = PICKUP_DELAY_TICKS; });
    collectPiles(pool, piles, playerAt(0, 0, 0), inv);
    CHECK(inv.slots[0].count == 64);
    CHECK(piles.size() == 1);
    ItemPile *p = nullptr;
    for(int i = 0; i < ItemPileStore::CAPACITY && !p; ++i) p = piles.get(i);
    REQUIRE(p != nullptr);
    unsigned left = 0;
    for(const auto &s : p->stacks) left += s.count;
    CHECK(left == 6 + 5);
}

TEST_CASE("piles come back after loading and despawn together with their record") {
    EntityPool pool; ItemPileStore piles; PlayerInventory inv;
    const ItemStack one[1] = { ItemStack(ITEM_DIAMOND, 2) };
    dropPile(pool, piles, 0, 0, 0, one, 1, inv);
    EntityPool fresh;                      // what a load starts with: no entities
    respawnPiles(fresh, piles);
    CHECK(fresh.count() == 1);

    fresh.forEach([](Entity &e) { e.age_ticks = DESPAWN_TICKS; });
    removeExpiredPiles(fresh, piles);
    CHECK(fresh.count() == 0);
    CHECK(piles.size() == 0);
}

TEST_CASE("an empty chest drops no pile") {
    EntityPool pool; ItemPileStore piles; PlayerInventory inv;
    CHECK(dropPile(pool, piles, 0, 0, 0, nullptr, 0, inv));
    CHECK(piles.size() == 0);
    CHECK(pool.count() == 0);
}

TEST_CASE("smithing upgrades diamond to netherite, keeping wear") {
    const ItemStack out = smithingResult(ItemStack(DIAMOND_PICK, 1, 400), ItemStack(ITEM_NETHERITE_INGOT, 3));
    CHECK(out.id == NETHERITE_PICK);
    CHECK(out.meta == 400);
    CHECK(out.count == 1);
    CHECK(smithingResult(ItemStack(IRON_SWORD, 1), ItemStack(ITEM_NETHERITE_INGOT, 1)).empty());
    CHECK(smithingResult(ItemStack(DIAMOND_PICK, 1), ItemStack(ITEM_DIAMOND, 1)).empty());
    CHECK(smithingResult(ItemStack(), ItemStack(ITEM_NETHERITE_INGOT, 1)).empty());
}

TEST_CASE("beds face where you look and put the head one block further") {
    CHECK(bedFacingFromYaw(0) == BLOCK_BACK);     // looking +z
    CHECK(bedFacingFromYaw(90) == BLOCK_RIGHT);   // +x
    CHECK(bedFacingFromYaw(180) == BLOCK_FRONT);  // -z
    CHECK(bedFacingFromYaw(270) == BLOCK_LEFT);   // -x
    CHECK(bedFacingFromYaw(350) == BLOCK_BACK);
    int dx, dz;
    sideOffset(BLOCK_RIGHT, dx, dz); CHECK(dx == 1); CHECK(dz == 0);
    sideOffset(BLOCK_FRONT, dx, dz); CHECK(dx == 0); CHECK(dz == -1);
    // the other half of a bed, from either half
    int ox, oz;
    bedOtherHalf(10, 20, bedData(BLOCK_LEFT, false), ox, oz); CHECK(ox == 9);  CHECK(oz == 20);
    bedOtherHalf(9, 20, bedData(BLOCK_LEFT, true), ox, oz);   CHECK(ox == 10); CHECK(oz == 20);
}

TEST_CASE("you can only sleep at night, and never in Torment") {
    CHECK(canSleep(6000, Difficulty::NORMAL) == SleepCheck::NOT_NIGHT);
    CHECK(canSleep(12000, Difficulty::NORMAL) == SleepCheck::NOT_NIGHT);
    CHECK(canSleep(12541, Difficulty::NORMAL) == SleepCheck::OK);
    CHECK(canSleep(18000, Difficulty::HARD) == SleepCheck::OK);
    CHECK(canSleep(23459, Difficulty::NORMAL) == SleepCheck::NOT_NIGHT);
    CHECK(canSleep(18000, Difficulty::TORMENT) == SleepCheck::TORMENT);
}

TEST_CASE("sleeping skips to morning and sets the spawn point") {
    world_state.time_of_day = 18000;
    player_state.spawn_is_bed = false;
    sleepInBed(7, 21, -3);
    CHECK(world_state.time_of_day == 0);
    CHECK(player_state.spawn_is_bed);
    CHECK(player_state.spawn_x == 7); CHECK(player_state.spawn_y == 21); CHECK(player_state.spawn_z == -3);
}
