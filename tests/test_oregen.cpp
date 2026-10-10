#include "doctest.h"
#include "oregen.h"

#include <cstring>

namespace {

typedef BLOCK_WDATA ChunkBlocks[8][8][8];

void fill(ChunkBlocks &b, BLOCK what)
{
    for(int x = 0; x < 8; ++x) for(int y = 0; y < 8; ++y) for(int z = 0; z < 8; ++z)
        b[x][y][z] = what;
}

struct Counts { int emerald = 0, gold = 0, debris = 0, debris_too_high = 0; };

Counts countMany(uint32_t seed)
{
    Counts c;
    ChunkBlocks b;
    for(int cx = -10; cx < 10; ++cx)
        for(int cz = -10; cz < 10; ++cz)
            for(int cy = 0; cy < 3; ++cy)
            {
                fill(b, BLOCK_STONE);
                generateOres(b, cx, cy, cz, seed);
                for(int x = 0; x < 8; ++x) for(int y = 0; y < 8; ++y) for(int z = 0; z < 8; ++z)
                {
                    const BLOCK k = getBLOCK(b[x][y][z]);
                    const int gy = cy * 8 + y;
                    if(k == BLOCK_EMERALD_ORE) ++c.emerald;
                    if(k == BLOCK_GOLD_ORE) ++c.gold;
                    if(k == BLOCK_ANCIENT_DEBRIS) { ++c.debris; if(gy > DEBRIS_MAX_Y) ++c.debris_too_high; }
                }
            }
    return c;
}

} // namespace

TEST_CASE("emerald is about as common as gold") {
    const Counts c = countMany(1234);
    CHECK(c.gold > 100);
    CHECK(c.emerald * 2 > c.gold);
    CHECK(c.emerald < c.gold * 2);
}

TEST_CASE("Ancient Debris is rare and only just above bedrock") {
    const Counts c = countMany(99);
    CHECK(c.debris > 5);
    CHECK(c.debris < c.gold / 2);
    CHECK(c.debris_too_high == 0);
}

TEST_CASE("only stone is replaced") {
    ChunkBlocks b;
    for(uint32_t seed = 0; seed < 200; ++seed)
    {
        fill(b, BLOCK_DIRT);
        b[3][1][3] = BLOCK_AIR;
        generateOres(b, 0, 0, static_cast<int>(seed), seed);
        for(int x = 0; x < 8; ++x) for(int y = 0; y < 8; ++y) for(int z = 0; z < 8; ++z)
        {
            const BLOCK expected = (x == 3 && y == 1 && z == 3) ? BLOCK_AIR : BLOCK_DIRT;
            REQUIRE(getBLOCK(b[x][y][z]) == expected);
        }
    }
}

TEST_CASE("the same world gives the same ores") {
    ChunkBlocks a, b;
    fill(a, BLOCK_STONE); fill(b, BLOCK_STONE);
    generateOres(a, 3, 0, -7, 555);
    generateOres(b, 3, 0, -7, 555);
    CHECK(std::memcmp(a, b, sizeof(a)) == 0);
}

TEST_CASE("nothing above the deep layers") {
    ChunkBlocks b;
    for(int i = 0; i < 300; ++i)
    {
        fill(b, BLOCK_STONE);
        generateOres(b, i, 4, -i, 7);   // y 32-39, above every ore's range
        for(int x = 0; x < 8; ++x) for(int y = 0; y < 8; ++y) for(int z = 0; z < 8; ++z)
            REQUIRE(getBLOCK(b[x][y][z]) == BLOCK_STONE);
    }
}
