#ifndef MINING_H
#define MINING_H

#include <cstdint>

#include "items.h"
#include "terrain.h"

// 1.8.8 block breaking (Minecraft Wiki "Breaking", Java Edition 1.8):
// each tick adds speed / hardness / 30 when the tool can harvest the block,
// or / 100 when it can't; the block breaks at 1. Speed is the tool's
// material speed when it suits the block, else 1 (swords: 1.5, 15 on cobweb).
// Underwater and in mid-air each divide speed by 5.

constexpr int MINING_NEVER = -1;          // bedrock, fluids
constexpr int MINING_COOLDOWN_TICKS = 5;  // 1.8.8's pause after each break

// Ticks to break the block; 0 = instantly, MINING_NEVER = can't.
int miningTicks(BLOCK_WDATA target, ItemId tool, bool in_water, bool on_ground);

// Whether breaking it with this tool gives its drop.
bool canHarvest(BLOCK_WDATA target, ItemId tool);

// What breaking it gives (empty when nothing). rng is any running state.
ItemStack blockDropFor(BLOCK_WDATA target, ItemId tool, uint32_t &rng);

// Wear from breaking one block: 1 for tools, 2 for swords, 0 for instant
// blocks and things that aren't tools.
int toolWearFor(ItemId tool, BLOCK_WDATA target);

// Adds wear; the stack empties when the tool's uses run out.
void wearTool(ItemStack &tool, int amount);

struct MiningState {
    bool active = false;
    int x = 0, y = 0, z = 0;
    BLOCK_WDATA block = 0;
    int progress = 0, needed = 0;
    int cooldown = 0;
};

// One game tick. Returns true on the tick the block breaks.
bool miningTick(MiningState &m, bool holding, bool has_target, int x, int y, int z,
                BLOCK_WDATA looked_at, ItemId tool, bool in_water, bool on_ground);

// 0-9 for the crack overlay, -1 when not mining.
int crackStage(const MiningState &m);

#endif // MINING_H
