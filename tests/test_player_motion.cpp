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
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(10), false);
    CHECK((box.low_z - z0).floor() >= 9); CHECK((box.low_z - z0).floor() <= 10);
}
TEST_CASE("jump only from the ground") {
    GridWorld w = floorWorld(); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false);
    CHECK(box.low_y > GLFix(128));
    GLFix y1 = box.low_y; GLFix vy1 = m.vy;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false);
    CHECK(m.vy < vy1); CHECK(box.low_y > y1);
}

static GridWorld stepWorld() {
    GridWorld w; for (int x = -3; x <= 3; ++x) w.solid.insert(std::make_tuple(x, 0, 0));
    w.solid.insert(std::make_tuple(0, 1, 1));
    return w;
}
TEST_CASE("auto-jump climbs a step when on") {
    GridWorld w = stepWorld(); AABB box = playerBoxAt(64, 128, 70); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), true);
    CHECK(box.low_y > GLFix(128));
}
TEST_CASE("no auto-jump when off") {
    GridWorld w = stepWorld(); AABB box = playerBoxAt(64, 128, 70); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), false);
    CHECK(box.low_y == GLFix(128));
}

// A pool: water in cells (x, 1..depth, z) over a stone floor at y 0.
static GridWorld poolWorld(int depth) {
    GridWorld w;
    for (int x = -6; x <= 6; ++x) for (int z = -6; z <= 6; ++z) {
        w.solid.insert(std::make_tuple(x, 0, z));
        for (int y = 1; y <= depth; ++y) w.water.insert(std::make_tuple(x, y, z));
    }
    return w;
}
static GLFix blocksPerTick(GLFix units) { return units / BLOCK_SIZE; }

TEST_CASE("you only swim with your body in the water") {
    GridWorld w = poolWorld(3);
    CHECK(w.touchesWater(swimBox(playerBoxAt(64, 128, 64))));
    CHECK(w.touchesWater(swimBox(playerBoxAt(64, 4 * 128 - 60, 64))));    // feet just under the surface
    CHECK_FALSE(w.touchesWater(swimBox(playerBoxAt(64, 4 * 128 - 40, 64))));
    CHECK_FALSE(poolWorld(0).touchesWater(swimBox(playerBoxAt(64, 128, 64))));
}

TEST_CASE("sinking in water settles at 2 blocks a second") {
    GridWorld w = poolWorld(30); AABB box = playerBoxAt(64, 25 * 128, 64); PlayerMotion m;
    m.vy = -60;   // falling in fast
    for (int i = 0; i < 40; ++i) playerTick(w, box, m, PlayerInput{0, 0, false}, GLFix(0), GLFix(20), false);
    CHECK(m.swimming);
    const GLFix y0 = box.low_y;
    playerTick(w, box, m, PlayerInput{0, 0, false}, GLFix(0), GLFix(20), false);
    const GLFix per_tick = blocksPerTick(y0 - box.low_y);
    CHECK(per_tick > GLFix(0.09f)); CHECK(per_tick < GLFix(0.11f));
}

TEST_CASE("holding jump swims up at 2 blocks a second") {
    GridWorld w = poolWorld(30); AABB box = playerBoxAt(64, 5 * 128, 64); PlayerMotion m;
    for (int i = 0; i < 40; ++i) playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(20), false);
    const GLFix y0 = box.low_y;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(20), false);
    const GLFix per_tick = blocksPerTick(box.low_y - y0);
    CHECK(per_tick > GLFix(0.09f)); CHECK(per_tick < GLFix(0.11f));
}

TEST_CASE("swimming up stops with the feet just under the surface") {
    GridWorld w = poolWorld(4); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m;
    GLFix highest = 0;
    for (int i = 0; i < 200; ++i) {
        playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(20), false);
        if (i > 100 && box.low_y > highest) highest = box.low_y;
    }
    // the surface is at y 5 (640): bobbing there, never out of the water
    CHECK(highest > GLFix(5 * 128 - 70)); CHECK(highest < GLFix(5 * 128));
}

TEST_CASE("walking in water is half speed and auto-jump is off") {
    GridWorld w = poolWorld(2); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m; m.on_ground = true;
    const GLFix z0 = box.low_z;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), true);
    CHECK(m.swimming);
    CHECK((box.low_z - z0).floor() >= 9); CHECK((box.low_z - z0).floor() <= 10);
}

TEST_CASE("pushing against a bank near the surface steps out of the water") {
    // Water 2 deep for z < 2; the bank at z >= 2 is level with the surface (top at y 3).
    GridWorld w;
    for (int x = -4; x <= 4; ++x) for (int z = -4; z <= 6; ++z) {
        w.solid.insert(std::make_tuple(x, 0, z));
        if (z < 2) { w.water.insert(std::make_tuple(x, 1, z)); w.water.insert(std::make_tuple(x, 2, z)); }
        else { w.solid.insert(std::make_tuple(x, 1, z)); w.solid.insert(std::make_tuple(x, 2, z)); }
    }
    AABB box = playerBoxAt(64, 128, 64 + 128 - 52); PlayerMotion m;   // against the bank, on the floor
    // Walking along the bottom gets nowhere...
    for (int i = 0; i < 20; ++i) playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), false);
    CHECK(box.low_y < GLFix(2 * 128));
    // ...but swimming up while pushing climbs onto the bank.
    for (int i = 0; i < 60; ++i) playerTick(w, box, m, PlayerInput{1, 0, true}, GLFix(0), GLFix(20), false);
    CHECK(box.low_y >= GLFix(3 * 128)); CHECK(box.low_z > GLFix(2 * 128));
}

TEST_CASE("a bank a block above the water can't be climbed from deep water") {
    GridWorld w;
    for (int x = -4; x <= 4; ++x) for (int z = -4; z <= 6; ++z) {
        w.solid.insert(std::make_tuple(x, 0, z));
        for (int y = 1; y <= 3; ++y) {
            if (z < 2 && y <= 2) w.water.insert(std::make_tuple(x, y, z));
            if (z >= 2) w.solid.insert(std::make_tuple(x, y, z));
        }
    }
    AABB box = playerBoxAt(64, 128, 64 + 128 - 52); PlayerMotion m;
    for (int i = 0; i < 80; ++i) playerTick(w, box, m, PlayerInput{1, 0, true}, GLFix(0), GLFix(20), false);
    CHECK(box.low_z < GLFix(2 * 128));
}
