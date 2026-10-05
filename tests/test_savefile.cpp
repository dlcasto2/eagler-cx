#include "doctest.h"
#include "savefile.h"
#include "gamestate.h"
#include "playerinventory.h"
#include "blockstorage.h"
#include "itempile.h"

#include <cstdio>

TEST_CASE("path helpers") {
    CHECK(backupPathFor("/documents/ndless/crafti.map.tns") == "/documents/ndless/crafti.map.bak.tns");
    CHECK(tempPathFor("/a/crafti.map.tns") == "/a/crafti.map.tmp.tns");
    CHECK(newWorldPathFor("/a/crafti.map.tns") == "/a/crafti.map.new.tns");
}
TEST_CASE("unreadable save is never overwritten") {
    CHECK(chooseSavePath("/a/w.map.tns", LoadResult::UNREADABLE) == "/a/w.map.new.tns");
    CHECK(chooseSavePath("/a/w.map.tns", LoadResult::OK) == "/a/w.map.tns");
    CHECK(chooseSavePath("/a/w.map.tns", LoadResult::MISSING) == "/a/w.map.tns");
}
TEST_CASE("survival section round-trips") {
    player_state.health = 7; player_state.mode = GameMode::SURVIVAL; world_state.difficulty = Difficulty::TORMENT;
    world_state.time_of_day = 18000; player_inventory.slots[30] = ItemStack(ITEM_EMERALD, 9);
    block_storage.create({1, 1, 1}, StorageKind::CHEST);
    gzFile o = gzopen("/tmp/crafti_sv.gz", "wb"); SaveWriter w(o); CHECK(writeSurvivalSection(w)); gzclose(o);
    player_state = PlayerState(); world_state = WorldState(); player_inventory.clear(); block_storage.clear();
    gzFile i = gzopen("/tmp/crafti_sv.gz", "rb"); SaveReader r(i); CHECK(readSurvivalSection(r)); gzclose(i);
    CHECK(player_state.health == 7); CHECK(player_state.mode == GameMode::SURVIVAL);
    CHECK(world_state.difficulty == Difficulty::TORMENT); CHECK(world_state.time_of_day == 18000);
    CHECK(player_inventory.slots[30].count == 9); CHECK(block_storage.find({1, 1, 1}) != nullptr);
}
TEST_CASE("v6 inventory converts to creative hotbar") {
    BLOCK_WDATA e[5] = { BLOCK_STONE, BLOCK_GRASS, BLOCK_PLANKS_NORMAL, getBLOCKWDATA(BLOCK_TORCH, 4), getBLOCKWDATA(BLOCK_FLOWER, 2) };
    PlayerInventory inv; PlayerState ps; ps.mode = GameMode::SURVIVAL;
    convertV6Inventory(e, 3, inv, ps);
    CHECK(inv.slots[3].id == BLOCK_TORCH); CHECK(inv.slots[3].meta == 4); CHECK(inv.slots[4].meta == 2);
    CHECK(inv.slots[0].count == 1); CHECK(inv.selected == 3); CHECK(ps.mode == GameMode::CREATIVE);
}
static bool writeB(gzFile f, void *) { return gzputs(f, "B") == 1; }
static bool writeFail(gzFile, void *) { return false; }
static void putV6(const char *p) { gzFile f = gzopen(p, "wb"); int v = 6; gzwrite(f, &v, sizeof v); gzclose(f); }
TEST_CASE("first v7 save keeps a backup") {
    putV6("/tmp/w.map.tns"); remove("/tmp/w.map.bak.tns");
    CHECK(replaceSaveSafely("/tmp/w.map.tns", writeB, nullptr));
    CHECK(readSaveVersion("/tmp/w.map.bak.tns") == 6);
    putV6("/tmp/w.map.tns"); CHECK(replaceSaveSafely("/tmp/w.map.tns", writeB, nullptr));
    CHECK(readSaveVersion("/tmp/w.map.bak.tns") == 6);
}
TEST_CASE("failed write keeps old save") {
    putV6("/tmp/w.map.tns");
    CHECK_FALSE(replaceSaveSafely("/tmp/w.map.tns", writeFail, nullptr));
    CHECK(readSaveVersion("/tmp/w.map.tns") == 6); CHECK(readSaveVersion("/tmp/w.map.tmp.tns") == -1);
}
