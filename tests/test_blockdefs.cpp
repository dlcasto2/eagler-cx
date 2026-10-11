#include "doctest.h"
#include "blockdefs.h"

#include <initializer_list>

TEST_CASE("stone needs a wooden pickaxe and drops cobblestone") {
    const BlockDef &d = blockDef(BLOCK_STONE);
    CHECK(d.hardness_x100 == 150); CHECK(d.best_tool == ToolKind::PICKAXE); CHECK(d.min_tier == 1);
    CHECK(dropItemFor(BLOCK_STONE) == BLOCK_COBBLESTONE);
}
TEST_CASE("ore tiers and drops") {
    CHECK(blockDef(BLOCK_IRON_ORE).min_tier == 2); CHECK(blockDef(BLOCK_DIAMOND_ORE).min_tier == 3);
    CHECK(dropItemFor(BLOCK_DIAMOND_ORE) == ITEM_DIAMOND);
    CHECK(blockDef(BLOCK_REDSTONE_ORE).drop.min == 4); CHECK(blockDef(BLOCK_REDSTONE_ORE).drop.max == 5);
}
TEST_CASE("bedrock is unbreakable and blast proof") {
    CHECK(blockDef(BLOCK_BEDROCK).hardness_x100 == UNBREAKABLE); CHECK(blockDef(BLOCK_BEDROCK).blast_proof);
}
TEST_CASE("light sources") {
    CHECK(blockDef(BLOCK_TORCH).light == 14); CHECK(blockDef(BLOCK_GLOWSTONE).light == 15);
    CHECK(blockDef(BLOCK_LAVA).light == 15); CHECK(blockDef(BLOCK_STONE).light == 0);
}
TEST_CASE("every existing block is defined") {
    for (int b = 1; b <= BLOCK_NORMAL_LAST; ++b)
        if (b != 30 && b != 32)   // reserved for wool and block of emerald
            CHECK(blockDef(b).defined);
    for (int b : {127, 128, 129, 130, 131, 132, 133, 134, 135, 146, 147, 148, 149, 150}) CHECK(blockDef(b).defined);
}
TEST_CASE("emerald ore and Ancient Debris") {
    const BlockDef &e = blockDef(BLOCK_EMERALD_ORE), &d = blockDef(BLOCK_ANCIENT_DEBRIS);
    CHECK(e.hardness_x100 == 300); CHECK(e.min_tier == 3); CHECK(e.drop.item == ITEM_EMERALD);
    CHECK(d.hardness_x100 == 3000); CHECK(d.min_tier == 4); CHECK(d.blast_proof);
    CHECK(d.drop.item == DROP_SELF);
}
