#include "doctest.h"
#include "biome.h"
#include "terrain.h"
#include "water.h"

#include <cstdlib>

namespace {

Climate climate(float t, float h, float hills = 0.4f)
{
    return Climate{ GLFix(t), GLFix(h), GLFix(hills) };
}

// The height every chunk used before biomes, written out again on its own.
int oldHeight(const PerlinNoise &noise, int x, int z)
{
    const GLFix n = noise.noise((GLFix(x & 7) / 8 + (x >> 3)) / 4, (GLFix(z & 7) / 8 + (z >> 3)) / 4, 10);
    return 8 + (n * 20).round();
}

} // namespace

TEST_CASE("climate picks the biome") {
    CHECK(biomeFor(climate(0.2f, 0.5f)) == Biome::ICE_PLAINS);
    CHECK(biomeFor(climate(0.38f, 0.2f)) == Biome::TAIGA);
    CHECK(biomeFor(climate(0.5f, 0.3f)) == Biome::PLAINS);
    CHECK(biomeFor(climate(0.5f, 0.5f)) == Biome::FOREST);
    CHECK(biomeFor(climate(0.5f, 0.7f)) == Biome::BIRCH_FOREST);
    CHECK(biomeFor(climate(0.7f, 0.3f)) == Biome::DESERT);
    CHECK(biomeFor(climate(0.7f, 0.5f)) == Biome::FOREST);       // hot but wet: no desert
    CHECK(biomeFor(climate(0.2f, 0.5f, 0.8f)) == Biome::EXTREME_HILLS);
    CHECK(biomeName(Biome::EXTREME_HILLS) != nullptr);
}

TEST_CASE("every biome gets a fair share of the world, in patches") {
    int count[static_cast<int>(Biome::COUNT)] = {}, total = 0, changes = 0;
    for(unsigned seed : { 7u, 99u, 31337u })
    {
        PerlinNoise noise(seed);
        for(int x = -1200; x < 1200; x += 6)
        {
            Biome prev = biomeAt(noise, x, -1200);
            for(int z = -1200; z < 1200; z += 6)
            {
                const Biome b = biomeAt(noise, x, z);
                ++count[static_cast<int>(b)];
                ++total;
                if(b != prev) ++changes;
                prev = b;
            }
        }
    }
    for(int b = 0; b < static_cast<int>(Biome::COUNT); ++b)
    {
        CAPTURE(biomeName(static_cast<Biome>(b)));
        CHECK(count[b] * 100 > total * 5);     // at least 5% each...
        CHECK(count[b] * 100 < total * 35);    // ...and none takes over
    }
    // Biomes come in patches: along a line the biome changes about every
    // 60 blocks or more, not every few.
    CHECK(changes * 8 < total);
}

TEST_CASE("outside the hills the ground is Crafti's old height, so old and new chunks meet") {
    PerlinNoise noise(1234);
    int same = 0, hills = 0, total = 0;
    for(int x = -300; x < 300; x += 3)
        for(int z = -300; z < 300; z += 3)
        {
            ++total;
            const int h = groundHeight(noise, x, z);
            CHECK(h >= oldHeight(noise, x, z));
            CHECK(h <= MAX_GROUND);
            if(h == oldHeight(noise, x, z)) ++same;
            if(biomeAt(noise, x, z) == Biome::EXTREME_HILLS) { ++hills; CHECK(h >= oldHeight(noise, x, z)); }
            else if(h != oldHeight(noise, x, z)) CHECK(h - oldHeight(noise, x, z) <= 4);   // only the hills' foothills
        }
    CHECK(same * 100 > total * 75);
    CHECK(hills > 0);
}

TEST_CASE("Extreme Hills are high and rise smoothly") {
    PerlinNoise noise(42);
    int high = 0, hill_columns = 0, biggest_step = 0;
    for(int x = -400; x < 400; ++x)
        for(int z = -400; z < 400; z += 9)
        {
            if(biomeAt(noise, x, z) != Biome::EXTREME_HILLS) continue;
            ++hill_columns;
            const int h = groundHeight(noise, x, z);
            if(h > SNOW_LINE) ++high;
            biggest_step = std::max(biggest_step, std::abs(h - groundHeight(noise, x + 1, z)));
        }
    REQUIRE(hill_columns > 100);
    CHECK(high * 10 > hill_columns);      // snowy peaks on part of them
    CHECK(biggest_step <= 4);             // crags, not walls
}

TEST_CASE("surfaces by biome") {
    CHECK(surfaceFor(Biome::PLAINS, 20, 0).top == BLOCK_GRASS);
    CHECK(surfaceFor(Biome::PLAINS, 20, 0).filler_depth == 4);   // grass on four dirt, as before
    const Surface desert = surfaceFor(Biome::DESERT, 20, 0);
    CHECK(desert.top == BLOCK_SAND); CHECK(desert.under == BLOCK_SANDSTONE); CHECK(desert.under_depth > 0);
    CHECK(surfaceFor(Biome::ICE_PLAINS, 20, 0).top == BLOCK_SNOWY_GRASS);
    CHECK(surfaceFor(Biome::EXTREME_HILLS, 22, 1).top == BLOCK_GRASS);
    CHECK(surfaceFor(Biome::EXTREME_HILLS, SNOW_LINE + 1, 1).top == BLOCK_SNOW);
    CHECK(surfaceFor(Biome::EXTREME_HILLS, 24, 4).top == BLOCK_STONE);     // cliffs are bare rock
}

TEST_CASE("biome decorations") {
    CHECK(biomeFeatures(Biome::DESERT).trees == 0);
    CHECK(biomeFeatures(Biome::DESERT).cacti > 0);
    CHECK(biomeFeatures(Biome::FOREST).trees > biomeFeatures(Biome::PLAINS).trees * 5);
    CHECK(biomeFeatures(Biome::TAIGA).spruce == 1000);
    CHECK(biomeFeatures(Biome::BIRCH_FOREST).birch == 1000);
    CHECK(biomeFeatures(Biome::ICE_PLAINS).frozen_water);
    CHECK_FALSE(biomeFeatures(Biome::PLAINS).frozen_water);
}

TEST_CASE("frozen lakes have ice on top") {
    BLOCK_WDATA b[8][8][8] = {};
    fillSeaColumn(b, 1, 1, 1, 12, true);
    CHECK(b[1][7][1] == BLOCK_ICE);              // global y 15, the surface
    CHECK(b[1][6][1] == WATER_SOURCE);
    CHECK(b[1][4][1] == WATER_SOURCE);
}

TEST_CASE("column hash spreads evenly") {
    int below = 0;
    for(int x = 0; x < 200; ++x)
        for(int z = 0; z < 200; ++z)
            if(columnHash(x, z, 5, 1) % 1000 < 100) ++below;
    CHECK(below > 3600); CHECK(below < 4400);    // 10% of 40000
    CHECK(columnHash(3, 4, 5, 1) != columnHash(3, 4, 5, 2));
}
