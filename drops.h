#ifndef DROPS_H
#define DROPS_H

#include "entity.h"
#include "playerinventory.h"

// Items lying in the world. They share the entity pool with mobs, so they
// never take more than MAX_DROPS slots; anything past that goes straight
// into the player's inventory instead of being lost.
constexpr int MAX_DROPS = 8;
constexpr int PICKUP_DELAY_TICKS = 10;   // 1.8.8: half a second after a block breaks

// Spawns (or merges into a nearby matching drop) at the given centre.
// Returns false only if it had to fall back to the inventory and that was full.
bool spawnDrop(EntityPool &pool, GLFix cx, GLFix cy, GLFix cz, ItemStack s, PlayerInventory &overflow);

int countDrops(const EntityPool &pool);

// Collects drops within reach of the player's box into the inventory:
// a block sideways and down, half a block up.
void collectDrops(EntityPool &pool, const AABB &player, PlayerInventory &inv);

#endif // DROPS_H
