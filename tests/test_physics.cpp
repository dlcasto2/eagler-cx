#include "doctest.h"
#include "grid_world.h"

TEST_CASE("falling stops on the ground") {
    GridWorld w; w.solid.insert(std::make_tuple(0, 0, 0));
    AABB box = playerBoxAt(64, 256, 64); MoveResult r{};
    for (int i = 0; i < 20; ++i) r = moveWithCollision(w, box, 0, GLFix(-32), 0);
    CHECK(r.on_ground); CHECK(box.low_y == GLFix(BLOCK_SIZE));
}
TEST_CASE("walls block one axis only") {
    GridWorld w; w.solid.insert(std::make_tuple(1, 1, 0));
    AABB box = playerBoxAt(64, 128, 64);
    MoveResult r = moveWithCollision(w, box, GLFix(40), 0, GLFix(10));
    CHECK(r.blocked_x); CHECK_FALSE(r.blocked_z); CHECK(box.low_z == GLFix(64 - 51 + 10));
}
TEST_CASE("auto-jump onto a one-block step") {
    GridWorld w; for (int x = -2; x <= 3; ++x) w.solid.insert(std::make_tuple(x, 0, 0));
    w.solid.insert(std::make_tuple(1, 1, 0));
    CHECK(shouldAutoJump(w, playerBoxAt(64, 128, 64), GLFix(20), 0, true, false));
}
TEST_CASE("no auto-jump into a two-block wall") {
    GridWorld w; w.solid.insert(std::make_tuple(1, 1, 0)); w.solid.insert(std::make_tuple(1, 2, 0));
    CHECK_FALSE(shouldAutoJump(w, playerBoxAt(64, 128, 64), GLFix(20), 0, true, false));
}
TEST_CASE("no auto-jump without headroom") {
    GridWorld w; w.solid.insert(std::make_tuple(1, 1, 0)); w.solid.insert(std::make_tuple(0, 3, 0));
    CHECK_FALSE(shouldAutoJump(w, playerBoxAt(64, 128, 64), GLFix(20), 0, true, false));
}
TEST_CASE("no auto-jump in water or mid-air") {
    GridWorld w; w.solid.insert(std::make_tuple(1, 1, 0));
    CHECK_FALSE(shouldAutoJump(w, playerBoxAt(64, 128, 64), GLFix(20), 0, false, false));
    CHECK_FALSE(shouldAutoJump(w, playerBoxAt(64, 128, 64), GLFix(20), 0, true, true));
}
