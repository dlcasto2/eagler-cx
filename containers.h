#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <cstdint>
#include <vector>

#include "blockstorage.h"
#include "gamestate.h"
#include "items.h"
#include "terrain.h"

// Rules for the blocks you use with 7: chests, the smithing table and beds.

// ---- chests: 27 stacks each, kept in block_storage by position
void chestPlaced(int x, int y, int z);
// Its non-empty stacks; the record is removed.
std::vector<ItemStack> chestBroken(int x, int y, int z);

// ---- smithing table: a diamond tool and a netherite ingot make the
// netherite tool, keeping its wear. Empty when the pair doesn't fit.
ItemStack smithingResult(const ItemStack &tool, const ItemStack &ingot);

// ---- beds: two blocks long, the head one block further in the facing direction.
// Data: facing (BLOCK_FRONT/BACK/LEFT/RIGHT) | BED_HEAD on the head half.
constexpr uint8_t BED_HEAD = 8;
constexpr uint8_t bedData(BLOCK_SIDE facing, bool head) { return static_cast<uint8_t>(facing | (head ? BED_HEAD : 0)); }
constexpr BLOCK_SIDE bedFacing(uint8_t data) { return static_cast<BLOCK_SIDE>(data & 3); }

// The facing for a player looking along this yaw (degrees, 0 = +z, 90 = +x).
BLOCK_SIDE bedFacingFromYaw(int yaw_degrees);
// One block in this horizontal direction.
void sideOffset(BLOCK_SIDE side, int &dx, int &dz);
// Where the other half of the bed at (x, z) with this data is.
void bedOtherHalf(int x, int z, uint8_t data, int &other_x, int &other_z);

// 1.8.8: beds work from 12541 to 23458. Torment beds never work.
enum class SleepCheck { OK, NOT_NIGHT, TORMENT };
SleepCheck canSleep(uint32_t time_of_day, Difficulty difficulty);
// Skips to morning and makes this bed the spawn point.
void sleepInBed(int x, int y, int z);

#endif // CONTAINERS_H
