#include "doctest.h"
#include "water.h"
#include "blockdefs.h"
#include "mining.h"

namespace {

typedef BLOCK_WDATA ChunkBlocks[8][8][8];

void clear(ChunkBlocks &b)
{
    for(int x = 0; x < 8; ++x) for(int y = 0; y < 8; ++y) for(int z = 0; z < 8; ++z)
        b[x][y][z] = BLOCK_AIR;
}

const BLOCK_WDATA FLOWING = getBLOCKWDATA(BLOCK_WATER, RANGE_WATER - 1);
const BLOCK_WDATA AIR = BLOCK_AIR;

} // namespace

TEST_CASE("sea columns fill with water sources up to sea level") {
    // Chunk y 1 holds global y 8-15; SEA_LEVEL 16 puts the surface at y 15.
    ChunkBlocks b; clear(b);
    fillSeaColumn(b, 2, 3, 1, 12);              // ground is y 0-11
    for(int y = 0; y < 4; ++y) CHECK(b[2][y][3] == AIR);
    for(int y = 4; y < 8; ++y) CHECK(b[2][y][3] == WATER_SOURCE);
    CHECK(b[3][5][3] == AIR);                   // other columns untouched
}

TEST_CASE("no water above sea level or over high ground") {
    ChunkBlocks b; clear(b);
    fillSeaColumn(b, 0, 0, 2, 12);              // chunk y 2 is global y 16-23
    fillSeaColumn(b, 1, 1, 1, SEA_LEVEL);       // ground level with the water
    fillSeaColumn(b, 2, 2, 1, SEA_LEVEL + 3);
    for(int y = 0; y < 8; ++y) { CHECK(b[0][y][0] == AIR); CHECK(b[1][y][1] == AIR); CHECK(b[2][y][2] == AIR); }
}

TEST_CASE("deep water spans chunk layers and keeps what's already there") {
    ChunkBlocks b; clear(b);
    fillSeaColumn(b, 0, 0, 0, 5);               // chunk y 0 is global y 0-7
    for(int y = 5; y < 8; ++y) CHECK(b[0][y][0] == WATER_SOURCE);
    CHECK(b[0][4][0] == AIR);

    clear(b);
    b[4][6][4] = BLOCK_LEAVES;
    fillSeaColumn(b, 4, 4, 1, 10);
    CHECK(b[4][6][4] == BLOCK_LEAVES);
    CHECK(b[4][7][4] == WATER_SOURCE);
}

TEST_CASE("sand under and beside the water, trees only on dry land") {
    CHECK(isSandColumn(SEA_LEVEL - 4)); CHECK(isSandColumn(SEA_LEVEL));
    CHECK_FALSE(isSandColumn(SEA_LEVEL + 1));
    CHECK_FALSE(isDryLand(SEA_LEVEL)); CHECK(isDryLand(SEA_LEVEL + 1));
}

TEST_CASE("two sources make a third (infinite water)") {
    const BLOCK_WDATA two[4] = { WATER_SOURCE, WATER_SOURCE, AIR, AIR };
    CHECK(becomesWaterSource(FLOWING, two, BLOCK_STONE, true));
    CHECK(becomesWaterSource(FLOWING, two, WATER_SOURCE, false));       // on top of a source
    CHECK_FALSE(becomesWaterSource(FLOWING, two, AIR, false));          // falling water
    CHECK_FALSE(becomesWaterSource(FLOWING, two, FLOWING, false));      // on flowing water

    const BLOCK_WDATA one[4] = { WATER_SOURCE, FLOWING, FLOWING, AIR };
    CHECK_FALSE(becomesWaterSource(FLOWING, one, BLOCK_STONE, true));

    const BLOCK_WDATA lava_sides[4] = { getBLOCKWDATA(BLOCK_LAVA, RANGE_LAVA), getBLOCKWDATA(BLOCK_LAVA, RANGE_LAVA), AIR, AIR };
    CHECK_FALSE(becomesWaterSource(FLOWING, lava_sides, BLOCK_STONE, true));
    CHECK_FALSE(becomesWaterSource(getBLOCKWDATA(BLOCK_LAVA, 1), two, BLOCK_STONE, true));   // lava never does
}

TEST_CASE("lava hardens where it meets water") {
    CHECK(lavaHardensInto(getBLOCKWDATA(BLOCK_LAVA, RANGE_LAVA), true) == BLOCK_OBSIDIAN);
    CHECK(lavaHardensInto(getBLOCKWDATA(BLOCK_LAVA, 2), true) == BLOCK_COBBLESTONE);
    CHECK(lavaHardensInto(getBLOCKWDATA(BLOCK_LAVA, 1), true) == BLOCK_COBBLESTONE);
    CHECK(lavaHardensInto(getBLOCKWDATA(BLOCK_LAVA, RANGE_LAVA), false) == BLOCK_AIR);
    CHECK(lavaHardensInto(WATER_SOURCE, true) == BLOCK_AIR);
    CHECK(LAVA_ON_WATER == BLOCK_STONE);
}

TEST_CASE("obsidian matches 1.8.8") {
    const BlockDef &d = blockDef(BLOCK_OBSIDIAN);
    CHECK(d.defined); CHECK(d.hardness_x100 == 5000); CHECK(d.min_tier == 4); CHECK(d.blast_proof);
    CHECK(dropItemFor(BLOCK_OBSIDIAN) == BLOCK_OBSIDIAN);
    // 9.4 s with a diamond pickaxe, 250 s by hand (and no drop)
    CHECK(miningTicks(BLOCK_OBSIDIAN, toolId(ToolMaterial::DIAMOND, ToolKind::PICKAXE), false, true) == 188);
    CHECK(miningTicks(BLOCK_OBSIDIAN, 0, false, true) == 5000);
    CHECK_FALSE(canHarvest(BLOCK_OBSIDIAN, toolId(ToolMaterial::IRON, ToolKind::PICKAXE)));
}

TEST_CASE("air lasts 15 seconds underwater and refills at the surface") {
    uint16_t air = MAX_AIR_TICKS;
    for(int i = 0; i < 100; ++i) air = nextAirTicks(air, true, true);
    CHECK(air == MAX_AIR_TICKS - 100);
    for(int i = 0; i < 400; ++i) air = nextAirTicks(air, true, true);
    CHECK(air == 0);                                        // stays empty
    CHECK(nextAirTicks(air, false, true) == MAX_AIR_TICKS);  // one breath refills it
    CHECK(nextAirTicks(120, true, false) == MAX_AIR_TICKS);  // creative doesn't use air
}

TEST_CASE("buckets scoop sources and pour into air or fluids") {
    CHECK(canScoop(WATER_SOURCE));
    CHECK_FALSE(canScoop(FLOWING));
    CHECK_FALSE(canScoop(getBLOCKWDATA(BLOCK_LAVA, RANGE_LAVA)));
    CHECK_FALSE(canScoop(BLOCK_STONE));

    CHECK(canPourInto(AIR)); CHECK(canPourInto(FLOWING)); CHECK(canPourInto(getBLOCKWDATA(BLOCK_LAVA, 1)));
    CHECK_FALSE(canPourInto(BLOCK_STONE)); CHECK_FALSE(canPourInto(BLOCK_TORCH));

    CHECK(aimsAtWater(ITEM_BUCKET));
    CHECK_FALSE(aimsAtWater(ITEM_WATER_BUCKET)); CHECK_FALSE(aimsAtWater(BLOCK_STONE));
}

TEST_CASE("bucket items") {
    CHECK(itemDef(ITEM_BUCKET).max_stack == 16);
    CHECK(itemDef(ITEM_WATER_BUCKET).max_stack == 1);
    CHECK(ItemStack(ITEM_BUCKET, 3).stacksWith(ItemStack(ITEM_BUCKET, 1)));
    CHECK_FALSE(ItemStack(ITEM_WATER_BUCKET, 1).stacksWith(ItemStack(ITEM_WATER_BUCKET, 1)));
}
