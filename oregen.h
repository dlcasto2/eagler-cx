#ifndef OREGEN_H
#define OREGEN_H

#include <cstdint>

#include "terrain.h"

// Extra ores for newly generated chunks: gold and emerald in the deep layers
// (emerald about as common as gold, much more than in 1.8.8) and rare
// Ancient Debris just above bedrock. Only stone is ever replaced, and the
// result depends only on the block's position and the world seed.
// Chunks loaded from a save never come through here.

constexpr int DEEP_ORE_MAX_Y = 24;   // global block y
constexpr int DEBRIS_MAX_Y = 3;      // bedrock is y 0

void generateOres(BLOCK_WDATA blocks[8][8][8], int chunk_x, int chunk_y, int chunk_z, uint32_t world_seed);

#endif // OREGEN_H
