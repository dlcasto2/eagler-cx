#include "doctest.h"
#include "stresstest.h"

TEST_CASE("dummies fill only free slots") {
    EntityPool p; CHECK(spawnStressDummies(p, GLFix(0), GLFix(5120), GLFix(0), 1) == 32);
    EntityPool q; for (int i = 0; i < 5; ++i) q.spawn(EntityType::MOB, AABB());
    CHECK(spawnStressDummies(q, GLFix(0), GLFix(5120), GLFix(0), 1) == 27);
}
TEST_CASE("dummies spawn 3 to 12 blocks away") {
    EntityPool p; spawnStressDummies(p, GLFix(0), GLFix(5120), GLFix(0), 7);
    p.forEach([](Entity &e) {
        int cx = ((e.box.low_x + e.box.high_x) / 2).floor(), cz = ((e.box.low_z + e.box.high_z) / 2).floor();
        int d2 = cx * cx + cz * cz;
        // GLFix integer truncation in the two scaling multiplies loses a few
        // units per axis, so allow a 1% slack on the 3-block minimum.
        CHECK(d2 >= (3 * 128 * 3 * 128 * 98) / 100); CHECK(d2 <= 12 * 128 * 12 * 128);
    });
}
TEST_CASE("brightness steps every 100 ticks") {
    CHECK(stressBrightnessForTick(0) == 7); CHECK(stressBrightnessForTick(100) == 6);
    CHECK(stressBrightnessForTick(799) == 0); CHECK(stressBrightnessForTick(800) == 7);
}
