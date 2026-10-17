#ifndef WATER_H
#define WATER_H

#include <cstdint>

#include "items.h"
#include "terrain.h"

// Water in survival: oceans and lakes, the 1.8.8 flow rules, air and buckets.
// Everything here is pure so the host tests can check it.

// ---- Oceans and lakes -------------------------------------------------------

// New chunks fill every column whose ground is lower than this with still
// water, so the water surface is the top of global block y SEA_LEVEL - 1.
constexpr int SEA_LEVEL = 16;

// Ground at or below the water surface is sand: seabeds, plus a beach where
// the land is level with the water.
constexpr bool isSandColumn(int ground_height) { return ground_height <= SEA_LEVEL; }

// Trees and flowers only grow on land above the beach.
constexpr bool isDryLand(int ground_height) { return ground_height > SEA_LEVEL; }

// Fills one column of a chunk with water sources, from the ground (the
// column's height in blocks) up to sea level. Only air is replaced.
void fillSeaColumn(BLOCK_WDATA blocks[8][8][8], int x, int z, int chunk_y, int ground_height);

// ---- Fluids -----------------------------------------------------------------

constexpr bool isFluid(BLOCK_WDATA b) { return getBLOCK(b) == BLOCK_WATER || getBLOCK(b) == BLOCK_LAVA; }
constexpr bool isWaterSource(BLOCK_WDATA b) { return getBLOCK(b) == BLOCK_WATER && getBLOCKDATA(b) == RANGE_WATER; }
constexpr bool isLavaSource(BLOCK_WDATA b) { return getBLOCK(b) == BLOCK_LAVA && getBLOCKDATA(b) == RANGE_LAVA; }
constexpr BLOCK_WDATA WATER_SOURCE = getBLOCKWDATA(BLOCK_WATER, RANGE_WATER);

// 1.8.8 infinite water: flowing water with two or more source blocks beside it
// becomes a source itself, if it rests on something solid or on another source.
bool becomesWaterSource(BLOCK_WDATA self, const BLOCK_WDATA sides[4], BLOCK_WDATA below, bool below_is_solid);

// 1.8.8 lava meeting water: lava touching water anywhere but underneath hardens,
// a source into obsidian and flowing lava into cobblestone. BLOCK_AIR = no change.
BLOCK lavaHardensInto(BLOCK_WDATA lava, bool touches_water);

// Lava flowing down onto water turns that water into stone.
constexpr BLOCK LAVA_ON_WATER = BLOCK_STONE;

// ---- Breathing --------------------------------------------------------------

// 1.8.8: 15 seconds of air. With your head underwater it runs down a tick at a
// time; as soon as your head is out it is full again. Creative never uses it.
constexpr uint16_t MAX_AIR_TICKS = 300;
uint16_t nextAirTicks(uint16_t air, bool head_underwater, bool survival);

// ---- Buckets ----------------------------------------------------------------

// An empty bucket scoops up a water source...
constexpr bool canScoop(BLOCK_WDATA target) { return isWaterSource(target); }
// ...and a water bucket pours a source into air or any other fluid.
constexpr bool canPourInto(BLOCK_WDATA b) { return getBLOCK(b) == BLOCK_AIR || isFluid(b); }

// Whether the selection ray should stop at water sources (for scooping) or
// pass through fluids to the block behind, as Minecraft's does.
constexpr bool aimsAtWater(ItemId held) { return held == ITEM_BUCKET; }

#endif // WATER_H
