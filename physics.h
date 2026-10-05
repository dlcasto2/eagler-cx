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
constexpr uint16_t DESPAWN_TICKS = 6000;   // 5 minutes at 20 ticks per second

#endif // PHYSICS_H
