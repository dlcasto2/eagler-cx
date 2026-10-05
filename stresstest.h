#ifndef STRESSTEST_H
#define STRESSTEST_H

#include <cstdint>

#include "entity.h"

// Worst-case load for the CX gate: a full pool of wandering dummies plus
// cycling darkness, with an on-screen readout. Built only with -D STRESS_TEST.

// Fills every free slot with dummies 3-12 blocks away horizontally, 2 above
// the player. Returns how many it spawned.
int spawnStressDummies(EntityPool &pool, GLFix px, GLFix py, GLFix pz, uint32_t seed);

// Steps brightness down one level every 100 ticks, wrapping 7..0.
int stressBrightnessForTick(uint32_t tick);

// Gives a dummy a new walking direction every 40 ticks.
void stressSteer(Entity &e, uint32_t tick, uint32_t seed);

#endif // STRESSTEST_H
