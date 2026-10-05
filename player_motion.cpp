#include "player_motion.h"

#include "fastmath.h"

GLFix velocityPerTick(GLFix per_frame, int fps)
{
    return per_frame * fps / 20;
}

GLFix accelPerTick(GLFix per_frame, int fps)
{
    return per_frame * fps * fps / 400;
}

void playerTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in,
                GLFix yaw, GLFix walk_per_tick, bool head_in_water, bool auto_jump)
{
    // Forward is (sin yaw, cos yaw); right is 90 degrees off.
    const GLFix fwd_x = fast_sin(yaw), fwd_z = fast_cos(yaw);
    const GLFix right_x = fast_sin((yaw + 90).normaliseAngle()), right_z = fast_cos((yaw + 90).normaliseAngle());

    const GLFix dx = (fwd_x * in.forward + right_x * in.strafe) * walk_per_tick;
    const GLFix dz = (fwd_z * in.forward + right_z * in.strafe) * walk_per_tick;

    bool jump = in.jump && (m.on_ground || head_in_water);
    if(auto_jump && !jump && shouldAutoJump(world, box, dx, dz, m.on_ground, head_in_water))
        jump = true;

    if(jump)
        m.vy = JUMP_VELOCITY;

    m.vy -= GRAVITY_PER_TICK;

    const MoveResult r = moveWithCollision(world, box, dx, m.vy, dz);
    if(r.blocked_y)
        m.vy = 0;
    m.on_ground = r.on_ground;
}
