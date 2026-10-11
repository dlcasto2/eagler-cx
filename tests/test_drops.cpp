#include "doctest.h"
#include "drops.h"

static AABB playerAt(GLFix x, GLFix y, GLFix z)
{
    return AABB(x - 51, y, z - 51, x + 51, y + 230, z + 51);
}

static void age(EntityPool &pool, int ticks)
{
    pool.forEach([ticks](Entity &e) { e.age_ticks = static_cast<uint16_t>(e.age_ticks + ticks); });
}

TEST_CASE("a drop spawns as a small item entity") {
    EntityPool pool; PlayerInventory inv;
    CHECK(spawnDrop(pool, 64, 64, 64, ItemStack(BLOCK_DIRT, 1), inv));
    CHECK(countDrops(pool) == 1);
    int seen = 0;
    pool.forEach([&seen](Entity &e) {
        CHECK(e.type == EntityType::DROPPED_ITEM);
        CHECK(e.item.id == BLOCK_DIRT);
        CHECK(e.box.high_x - e.box.low_x < GLFix(64));
        ++seen;
    });
    CHECK(seen == 1);
}

TEST_CASE("drops next to each other merge into one stack") {
    EntityPool pool; PlayerInventory inv;
    spawnDrop(pool, 64, 64, 64, ItemStack(BLOCK_DIRT, 1), inv);
    spawnDrop(pool, 100, 64, 64, ItemStack(BLOCK_DIRT, 3), inv);
    CHECK(countDrops(pool) == 1);
    pool.forEach([](Entity &e) { CHECK(e.item.count == 4); });
    spawnDrop(pool, 100, 64, 64, ItemStack(BLOCK_SAND, 1), inv);   // different item
    spawnDrop(pool, 2000, 64, 64, ItemStack(BLOCK_DIRT, 1), inv);  // far away
    CHECK(countDrops(pool) == 3);
}

TEST_CASE("drops respect MAX_DROPS and never vanish") {
    EntityPool pool; PlayerInventory inv;
    for(int i = 0; i < MAX_DROPS + 3; ++i)
        spawnDrop(pool, i * 1000, 64, 64, ItemStack(ITEM_STICK, 1), inv);
    CHECK(countDrops(pool) == MAX_DROPS);
    CHECK(inv.count(ITEM_STICK, 0) == 3);   // the overflow went straight to the player
}

TEST_CASE("drops leave mob slots free") {
    CHECK(MAX_DROPS <= EntityPool::CAPACITY / 3);
}

TEST_CASE("pickup waits a moment, then collects within reach") {
    EntityPool pool; PlayerInventory inv;
    spawnDrop(pool, 64, 64, 64, ItemStack(BLOCK_DIRT, 2), inv);
    collectDrops(pool, playerAt(64, 0, 64), inv);
    CHECK(countDrops(pool) == 1);                // too fresh
    age(pool, PICKUP_DELAY_TICKS);
    collectDrops(pool, playerAt(64 + 128 * 4, 0, 64), inv);
    CHECK(countDrops(pool) == 1);                // too far
    collectDrops(pool, playerAt(64 + 128, 0, 64), inv);
    CHECK(countDrops(pool) == 0);
    CHECK(inv.count(BLOCK_DIRT, 0) == 2);
}

TEST_CASE("pickup merges and keeps what doesn't fit") {
    EntityPool pool; PlayerInventory inv;
    for(auto &s : inv.slots) s = ItemStack(ITEM_STICK, 64);
    inv.slots[5] = ItemStack(BLOCK_DIRT, 60);
    spawnDrop(pool, 64, 64, 64, ItemStack(BLOCK_DIRT, 10), inv);
    age(pool, PICKUP_DELAY_TICKS);
    collectDrops(pool, playerAt(64, 0, 64), inv);
    CHECK(inv.slots[5].count == 64);
    CHECK(countDrops(pool) == 1);
    pool.forEach([](Entity &e) { CHECK(e.item.count == 6); });
}

TEST_CASE("a drop that fell into the hole next to you is collected") {
    EntityPool pool; PlayerInventory inv;
    // standing on the surface at y = 1280; the hole's floor is one block lower
    const GLFix floor_y = 1280 - BLOCK_SIZE;
    spawnDrop(pool, 64 + BLOCK_SIZE, floor_y + 16, 64, ItemStack(BLOCK_DIRT, 1), inv);
    age(pool, PICKUP_DELAY_TICKS);
    collectDrops(pool, playerAt(64, 1280, 64), inv);
    CHECK(inv.count(BLOCK_DIRT, 0) == 1);
}
