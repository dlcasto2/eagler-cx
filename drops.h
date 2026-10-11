#ifndef DROPS_H
#define DROPS_H

#include "entity.h"
#include "playerinventory.h"
#include "itempile.h"

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

// ---- Item piles: many stacks in one place using one entity slot (a broken
// chest's contents now, everything you carried after death later). The
// contents live in an ItemPileStore, which is saved with the world.

// Spawns a pile of the non-empty stacks at the given centre. Falls back to
// separate drops (and the inventory) if the pool or the store is full.
bool dropPile(EntityPool &pool, ItemPileStore &piles, GLFix cx, GLFix cy, GLFix cz,
              const ItemStack *stacks, int n, PlayerInventory &overflow);

// Gives the player as much of each pile in reach as fits; empty piles go away.
void collectPiles(EntityPool &pool, ItemPileStore &piles, const AABB &player, PlayerInventory &inv);

// After loading: an entity for every stored pile (entities aren't saved).
void respawnPiles(EntityPool &pool, ItemPileStore &piles);

// Removes piles older than DESPAWN_TICKS, entity and contents together,
// and keeps the stored age in step for saving.
void removeExpiredPiles(EntityPool &pool, ItemPileStore &piles);

#endif // DROPS_H
