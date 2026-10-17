#include "player_motion.h"

#include "fastmath.h"
#include "terrain.h"

GLFix velocityPerTick(GLFix per_frame, int fps)
{
    return per_frame * fps / 20;
}

GLFix accelPerTick(GLFix per_frame, int fps)
{
    return per_frame * fps * fps / 400;
}

static AABB shifted(const AABB &box, GLFix dx, GLFix dy, GLFix dz)
{
    return AABB(box.low_x + dx, box.low_y + dy, box.low_z + dz, box.high_x + dx, box.high_y + dy, box.high_z + dz);
}

AABB swimBox(const AABB &b)
{
    const GLFix shrink = GLFix(BLOCK_SIZE) * 2 / 5;
    return AABB(b.low_x, b.low_y + shrink, b.low_z, b.high_x, b.high_y - shrink, b.high_z);
}

// 1.8.8 entities step up to 0.6 blocks; Crafti uses auto-jump on land, but
// in water this is what lets you climb out onto a bank.
static const GLFix SWIM_STEP = GLFix(BLOCK_SIZE) * 3 / 5;

static void swimTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in, GLFix dx, GLFix dz)
{
    // About half walking speed: 1.8.8 settles at 2 blocks/s in water.
    dx = dx / 2;
    dz = dz / 2;

    if(in.jump)
        m.vy += SWIM_UP_PER_TICK;

    const MoveResult r = moveWithCollision(world, box, dx, m.vy, dz);
    if(r.blocked_y)
        m.vy = 0;
    m.on_ground = r.on_ground;

    // Pushing against a bank that's at most SWIM_STEP above your feet steps onto it.
    if(r.blocked_x || r.blocked_z)
    {
        const GLFix sx = r.blocked_x ? dx : GLFix(0), sz = r.blocked_z ? dz : GLFix(0);
        const AABB up = shifted(box, 0, SWIM_STEP, 0), over = shifted(box, sx, SWIM_STEP, sz);
        if(!world.intersects(up) && !world.intersects(over))
        {
            box = over;
            m.vy = 0;
            return;
        }
    }

    m.vy = m.vy * 4 / 5 - SWIM_SINK_PER_TICK;
}

void playerTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in,
                GLFix yaw, GLFix walk_per_tick, bool auto_jump)
{
    // Forward is (sin yaw, cos yaw); right is 90 degrees off.
    const GLFix fwd_x = fast_sin(yaw), fwd_z = fast_cos(yaw);
    const GLFix right_x = fast_sin((yaw + 90).normaliseAngle()), right_z = fast_cos((yaw + 90).normaliseAngle());

    const GLFix dx = (fwd_x * in.forward + right_x * in.strafe) * walk_per_tick;
    const GLFix dz = (fwd_z * in.forward + right_z * in.strafe) * walk_per_tick;

    m.swimming = world.touchesWater(swimBox(box));
    if(m.swimming)
        return swimTick(world, box, m, in, dx, dz);

    bool jump = in.jump && m.on_ground;
    if(auto_jump && !jump && shouldAutoJump(world, box, dx, dz, m.on_ground, false))
        jump = true;

    if(jump)
        m.vy = JUMP_VELOCITY;

    m.vy -= GRAVITY_PER_TICK;

    const MoveResult r = moveWithCollision(world, box, dx, m.vy, dz);
    if(r.blocked_y)
        m.vy = 0;
    m.on_ground = r.on_ground;
}
