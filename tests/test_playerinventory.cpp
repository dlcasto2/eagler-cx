#include "doctest.h"
#include "playerinventory.h"

TEST_CASE("add merges before using empty slots") {
    PlayerInventory inv; inv.slots[3] = ItemStack(ITEM_COAL, 60);
    CHECK(inv.add(ItemStack(ITEM_COAL, 10)) == 0);
    CHECK(inv.slots[3].count == 64); CHECK(inv.slots[0].id == ITEM_COAL); CHECK(inv.slots[0].count == 6);
}
TEST_CASE("full inventory reports leftovers") {
    PlayerInventory inv; for (auto &s : inv.slots) s = ItemStack(ITEM_STICK, 64);
    CHECK(inv.add(ItemStack(ITEM_COAL, 5)) == 5);
}
TEST_CASE("tools never stack") {
    PlayerInventory inv; ItemId p = toolId(ToolMaterial::WOOD, ToolKind::PICKAXE);
    inv.add(ItemStack(p, 1)); inv.add(ItemStack(p, 1));
    CHECK(inv.slots[0].count == 1); CHECK(inv.slots[1].id == p);
}
TEST_CASE("remove and count across slots") {
    PlayerInventory inv; inv.slots[0] = ItemStack(ITEM_COAL, 5); inv.slots[20] = ItemStack(ITEM_COAL, 7);
    CHECK(inv.count(ITEM_COAL, 0) == 12); CHECK(inv.remove(ITEM_COAL, 0, 9) == 9);
    CHECK(inv.count(ITEM_COAL, 0) == 3); CHECK(inv.slots[20].empty()); CHECK(inv.slots[0].count == 3);
}
TEST_CASE("block variants are distinct") {
    PlayerInventory inv;
    inv.add(ItemStack::ofBlock(getBLOCKWDATA(BLOCK_FLOWER, 0)));
    inv.add(ItemStack::ofBlock(getBLOCKWDATA(BLOCK_FLOWER, 1)));
    CHECK(inv.slots[0].meta == 0); CHECK(inv.slots[1].meta == 1);
}
