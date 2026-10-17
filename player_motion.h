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
    bool swimming = false;   // the body was in water this tick
};

// The part of the player that has to be in water to swim: as in 1.8.8, the
// box less 0.4 blocks at the top and bottom. Rising past the surface ends
// the swim with your feet about 0.4 blocks under it.
AABB swimBox(const AABB &player_box);

// One tick of player physics: walk by (forward, strafe) at yaw, jump, gravity,
// then collision. auto_jump hops one-block steps (Task 12). In water the
// player swims instead (see SWIM_SINK_PER_TICK), and pushing against a bank
// up to 0.6 blocks above the feet steps onto it, as 1.8.8 entities do.
void playerTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in,
                GLFix yaw, GLFix walk_per_tick, bool auto_jump);

#endif // PLAYER_MOTION_H
