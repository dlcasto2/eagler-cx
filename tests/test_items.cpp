#include "doctest.h"
#include "items.h"

TEST_CASE("block items keep id and data") {
    ItemStack s = ItemStack::ofBlock(getBLOCKWDATA(BLOCK_FLOWER, 2));
    CHECK(s.id == BLOCK_FLOWER); CHECK(s.meta == 2); CHECK(isBlockItem(s.id));
    CHECK(s.toBlock() == getBLOCKWDATA(BLOCK_FLOWER, 2));
}
TEST_CASE("tool stats match 1.8.8") {
    const ItemDef &p = itemDef(toolId(ToolMaterial::WOOD, ToolKind::PICKAXE));
    CHECK(p.mining_speed == 2); CHECK(p.durability == 59); CHECK(p.harvest_tier == 1); CHECK(p.max_stack == 1);
    CHECK(itemDef(toolId(ToolMaterial::GOLD, ToolKind::PICKAXE)).mining_speed == 12);
    CHECK(itemDef(toolId(ToolMaterial::DIAMOND, ToolKind::SWORD)).attack_damage == 8);
    CHECK(itemDef(toolId(ToolMaterial::NETHERITE, ToolKind::SWORD)).durability == 2031);
}
TEST_CASE("food values match 1.8.8") {
    CHECK(itemDef(ITEM_BREAD).food_hunger == 5); CHECK(itemDef(ITEM_BREAD).food_saturation_x10 == 60);
    CHECK(itemDef(ITEM_BEEF_COOKED).food_hunger == 8); CHECK(itemDef(ITEM_ROTTEN_FLESH).food_saturation_x10 == 8);
}
TEST_CASE("stacking rules") {
    CHECK(ItemStack(ITEM_COAL, 10).stacksWith(ItemStack(ITEM_COAL, 5)));
    ItemStack axe(toolId(ToolMaterial::WOOD, ToolKind::AXE), 1);
    CHECK_FALSE(axe.stacksWith(axe));
    CHECK(itemDef(ITEM_COAL).max_stack == 64);
}
TEST_CASE("every defined item has a name") {
    for (ItemId id = FIRST_ITEM; id <= ITEM_LAST; ++id)
        if (itemDef(id).max_stack) CHECK(itemDef(id).name != nullptr);
}
