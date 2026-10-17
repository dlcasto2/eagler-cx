#ifndef PHYSICS_H
#define PHYSICS_H

#include <cstdint>

#include "aabb.h"

// Anything that can say whether a box hits solid blocks.
class CollisionQuery
{
public:
    virtual ~CollisionQuery() {}
    virtual bool intersects(const AABB &box) const = 0;
    // Whether any block the box overlaps is water.
    virtual bool touchesWater(const AABB &/*box*/) const { return false; }
};

struct MoveResult {
    bool blocked_x, blocked_y, blocked_z;
    bool on_ground;     // y was blocked while moving down
};

// Same rule as Crafti's original player code: move x, then z, then y; an axis
// whose move would intersect is not applied.
MoveResult moveWithCollision(const CollisionQuery &world, AABB &box, GLFix dx, GLFix dy, GLFix dz);

// True when walking (dx, dz) runs into a one-block step that can be climbed:
// on the ground, not in water, blocked ahead, and free one block higher both
// here and ahead.
bool shouldAutoJump(const CollisionQuery &world, const AABB &box, GLFix dx, GLFix dz, bool on_ground, bool in_water);

extern const int V13_FPS;
extern const GLFix GRAVITY_PER_TICK, JUMP_VELOCITY;

// Swimming, from 1.8.8's water movement: each tick vertical speed keeps 80%
// and loses 0.02 blocks/tick, while holding jump adds 0.04. That settles at
// 2 blocks/s sinking or rising. Walking speed is roughly halved.
extern const GLFix SWIM_SINK_PER_TICK, SWIM_UP_PER_TICK;

constexpr uint16_t DESPAWN_TICKS = 6000;   // 5 minutes at 20 ticks per second

#endif // PHYSICS_H
