#ifndef PLAYER_MOTION_H
#define PLAYER_MOTION_H

#include <cstdint>

#include "physics.h"

// Crafti 1.3 moved the player per frame; the game now ticks at 20/s. These
// scale the per-frame distances by the old frame rate so motion feels the same.
GLFix velocityPerTick(GLFix per_frame, int fps);   // per_frame * fps / 20
GLFix accelPerTick(GLFix per_frame, int fps);      // per_frame * fps * fps / 400

struct PlayerInput {
    int8_t forward;   // -1, 0, 1
    int8_t strafe;
    bool jump;
};

struct PlayerMotion {
    GLFix vy = 0;
    bool on_ground = false;
};

// One tick of player physics: walk by (forward, strafe) at yaw, jump, gravity,
// then collision. auto_jump hops one-block steps (Task 12).
void playerTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in,
                GLFix yaw, GLFix walk_per_tick, bool head_in_water, bool auto_jump);

#endif // PLAYER_MOTION_H
