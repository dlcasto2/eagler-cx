#ifndef FURNACE_H
#define FURNACE_H

#include "blockstorage.h"

// Furnace rules (1.8.8): each item cooks for 200 ticks (10 s) while fuel burns.
// Slots: input, fuel, output.
constexpr int FURNACE_INPUT = 0, FURNACE_FUEL = 1, FURNACE_OUTPUT = 2;
constexpr uint16_t FURNACE_COOK_TICKS = 200;

// What one of these smelts into (empty if it doesn't smelt).
ItemStack smeltResult(const ItemStack &in);
// How long one of these burns, in ticks (0 if it isn't fuel).
// Coal and charcoal 1600 (8 items), logs, planks, crafting tables and
// chests 300 (1.5 items), wooden tools 200, sticks and saplings 100.
uint16_t fuelTicks(const ItemStack &s);

// Whether the input can cook into the output slot right now.
bool furnaceCanSmelt(const StorageRecord &r);
// One game tick: burn fuel, light new fuel when there's something to cook,
// advance cooking, and let progress cool off when the fire is out.
void furnaceTick(StorageRecord &r);

inline bool furnaceBurning(const StorageRecord &r) { return r.burn_ticks_left > 0; }

void furnacePlaced(int x, int y, int z);

#endif // FURNACE_H
