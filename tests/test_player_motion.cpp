#include "doctest.h"
#include "player_motion.h"
#include "grid_world.h"

static GridWorld floorWorld() {
    GridWorld w;
    for (int x = -3; x <= 3; ++x) for (int z = -3; z <= 3; ++z) w.solid.insert(std::make_tuple(x, 0, z));
    return w;
}
TEST_CASE("tick conversion") {
    CHECK(velocityPerTick(GLFix(20), 10) == GLFix(10));
    CHECK(velocityPerTick(GLFix(50), 20) == GLFix(50));
    CHECK(accelPerTick(GLFix(5), 10) == GLFix(5) / 4);
}
TEST_CASE("walking forward at yaw 0 moves along +z") {
    GridWorld w = floorWorld(); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m; m.on_ground = true;
    GLFix z0 = box.low_z;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(10), false, false);
    CHECK((box.low_z - z0).floor() >= 9); CHECK((box.low_z - z0).floor() <= 10);
}
TEST_CASE("jump only from the ground") {
    GridWorld w = floorWorld(); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false, false);
    CHECK(box.low_y > GLFix(128));
    GLFix y1 = box.low_y; GLFix vy1 = m.vy;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false, false);
    CHECK(m.vy < vy1); CHECK(box.low_y > y1);
}
TEST_CASE("water allows jumping mid-air") {
    GridWorld w; AABB box = playerBoxAt(64, 512, 64); PlayerMotion m;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), true, false);
    CHECK(box.low_y > GLFix(512));
}

static GridWorld stepWorld() {
    GridWorld w; for (int x = -3; x <= 3; ++x) w.solid.insert(std::make_tuple(x, 0, 0));
    w.solid.insert(std::make_tuple(0, 1, 1));
    return w;
}
TEST_CASE("auto-jump climbs a step when on") {
    GridWorld w = stepWorld(); AABB box = playerBoxAt(64, 128, 70); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), false, true);
    CHECK(box.low_y > GLFix(128));
}
TEST_CASE("no auto-jump when off") {
    GridWorld w = stepWorld(); AABB box = playerBoxAt(64, 128, 70); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), false, false);
    CHECK(box.low_y == GLFix(128));
}
