#include "doctest.h"
#include "blockstorage.h"
#include "itempile.h"

static gzFile openTmp(const char *mode) { return gzopen("/tmp/crafti_storage_test.gz", mode); }

TEST_CASE("storage records have the right sizes") {
    BlockStorage s;
    CHECK(s.create({1, 2, 3}, StorageKind::FURNACE).slots.size() == 3);
    CHECK(s.create({4, 5, 6}, StorageKind::CHEST).slots.size() == 27);
    s.erase({1, 2, 3}); CHECK(s.find({1, 2, 3}) == nullptr); CHECK(s.size() == 1);
}
TEST_CASE("storage round-trips") {
    BlockStorage s; StorageRecord &f = s.create({-7, 20, 9}, StorageKind::FURNACE);
    f.slots[0] = ItemStack(BLOCK_IRON_ORE, 12); f.slots[1] = ItemStack(ITEM_COAL, 3); f.cook_ticks = 150;
    s.create({0, 0, 0}, StorageKind::CHEST).slots[26] = ItemStack(ITEM_DIAMOND, 2);
    gzFile o = openTmp("wb"); SaveWriter w(o); CHECK(s.save(w)); gzclose(o);
    BlockStorage t; gzFile i = openTmp("rb"); SaveReader r(i); CHECK(t.load(r)); gzclose(i);
    REQUIRE(t.find({-7, 20, 9}) != nullptr);
    CHECK(t.find({-7, 20, 9})->slots[1].count == 3); CHECK(t.find({-7, 20, 9})->cook_ticks == 150);
    CHECK(t.find({0, 0, 0})->slots[26].id == ITEM_DIAMOND);
}
TEST_CASE("storage load rejects a bad kind") {
    gzFile o = openTmp("wb"); SaveWriter w(o); w.u32(1); w.i32(0); w.i32(0); w.i32(0); w.u8(9); gzclose(o);
    BlockStorage t; gzFile i = openTmp("rb"); SaveReader r(i); CHECK_FALSE(t.load(r)); gzclose(i);
}
TEST_CASE("item piles round-trip and cap at 32") {
    ItemPileStore p; ItemPile pile{}; pile.x = GLFix(300); pile.stacks[35] = ItemStack(ITEM_BREAD, 4);
    CHECK(p.add(pile) == 0);
    gzFile o = openTmp("wb"); SaveWriter w(o); CHECK(p.save(w)); gzclose(o);
    ItemPileStore q; gzFile i = openTmp("rb"); SaveReader r(i); CHECK(q.load(r)); gzclose(i);
    CHECK(q.get(0)->stacks[35].count == 4); CHECK(q.get(0)->x == GLFix(300));
    for (int k = 1; k < 32; ++k) CHECK(p.add(pile) == k);
    CHECK(p.add(pile) == -1);
}
