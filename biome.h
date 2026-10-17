#ifndef BIOME_H
#define BIOME_H

#include <cstdint>

#include "perlinnoise.h"

// Biomes, named as in 1.8.8. Each column's biome comes from three slow noise
// fields (temperature, humidity and hilliness) of the world seed, so nothing
// about it is saved and it's the same every time a column is generated.
enum class Biome : uint8_t {
    PLAINS = 0, FOREST, BIRCH_FOREST, TAIGA, ICE_PLAINS, DESERT, EXTREME_HILLS,
    COUNT
};

const char *biomeName(Biome b);

// The three fields, each roughly 0..1 with half the world below 0.5.
struct Climate {
    GLFix temperature, humidity, hilliness;
};

// Thresholds, picked from the fields' measured spread so each biome gets 8-23%
// of the land (see tests/test_biome.cpp):
//   hilliness >= HILLS_START                      Extreme Hills
//   temperature < ICE_BELOW                       Ice Plains
//   temperature < TAIGA_BELOW                     Taiga
//   temperature >= DESERT_FROM, humidity < DRY    Desert
//   otherwise by humidity: < DRY Plains, < WET Forest, else Birch Forest
Biome biomeFor(const Climate &c);

// Climate and biome of the column at global block (x, z).
Climate climateAt(const PerlinNoise &noise, int x, int z);
Biome biomeAt(const PerlinNoise &noise, int x, int z);

// Ground height of the column (blocks from the bottom of the world): Crafti's
// original rolling terrain, raised where the hilliness field is high, up to
// MAX_GROUND. Outside the hills it's exactly the original height, so new
// chunks meet old ones without a step.
constexpr int MAX_GROUND = 33;
int groundHeight(const PerlinNoise &noise, int x, int z);

// Extreme Hills above this height get a cap of snow.
constexpr int SNOW_LINE = 29;

// How the ground at a column is made up, top to bottom.
struct Surface {
    uint8_t top;        // the top block
    uint8_t filler;     // the next few blocks
    uint8_t filler_depth;
    uint8_t under;      // below the filler, above the stone (desert sandstone), or 0
    uint8_t under_depth;
};

// steepness: the largest height difference to the four neighbouring columns.
Surface surfaceFor(Biome b, int ground_height, int steepness);

// Decorations per 1000 columns of land.
struct BiomeFeatures {
    uint16_t trees;      // tree trunks
    uint16_t birch;      // of those, how many in 1000 are birch
    uint16_t spruce;     // ... and spruce
    uint16_t flowers;
    uint16_t cacti;
    uint16_t dead_bushes;
    bool frozen_water;   // lakes freeze over
};
const BiomeFeatures &biomeFeatures(Biome b);

// A well-mixed hash of a column and the seed, for deciding decorations.
uint32_t columnHash(int x, int z, uint32_t seed, uint32_t salt);

#endif // BIOME_H
