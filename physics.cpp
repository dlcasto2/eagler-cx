#include "physics.h"

#include "terrain.h"
#include "player_motion.h"

// V13_FPS is the measured Crafti 1.3 frame rate; until the user records it on
// their CX it stays at the plan's default of 15 (see docs/baseline.md).
const int V13_FPS = 15;

// Crafti 1.3 used gravity 5 and jump velocity 50 *per frame*; rescaled to ticks.
const GLFix GRAVITY_PER_TICK = accelPerTick(5, V13_FPS);
const GLFix JUMP_VELOCITY = velocityPerTick(50, V13_FPS);

static AABB shifted(const AABB &box, GLFix dx, GLFix dy, GLFix dz)
{
    return AABB(box.low_x + dx, box.low_y + dy, box.low_z + dz, box.high_x + dx, box.high_y + dy, box.high_z + dz);
}

MoveResult moveWithCollision(const CollisionQuery &world, AABB &box, GLFix dx, GLFix dy, GLFix dz)
{
    MoveResult r{false, false, false, false};

    if(dx != GLFix(0))
    {
        const AABB moved = shifted(box, dx, 0, 0);
        if(world.intersects(moved))
            r.blocked_x = true;
        else
            box = moved;
    }

    if(dz != GLFix(0))
    {
        const AABB moved = shifted(box, 0, 0, dz);
        if(world.intersects(moved))
            r.blocked_z = true;
        else
            box = moved;
    }

    if(dy != GLFix(0))
    {
        const AABB moved = shifted(box, 0, dy, 0);
        if(world.intersects(moved))
        {
            r.blocked_y = true;
            r.on_ground = dy < GLFix(0);
        }
        else
            box = moved;
    }

    return r;
}

bool shouldAutoJump(const CollisionQuery &world, const AABB &box, GLFix dx, GLFix dz, bool on_ground, bool in_water)
{
    if(!on_ground || in_water)
        return false;

    if(!world.intersects(shifted(box, dx, 0, dz)))
        return false;   // nothing in the way

    return !world.intersects(shifted(box, 0, BLOCK_SIZE, 0))
        && !world.intersects(shifted(box, dx, BLOCK_SIZE, dz));
}
