#include "doctest.h"
#include "gameclock.h"

TEST_CASE("one tick per 50 ms at 1 kHz") {
    GameClock c(1000); c.reset(0);
    CHECK(c.ticksDue(49) == 0); CHECK(c.ticksDue(50) == 1); CHECK(c.ticksDue(100) == 1);
}
TEST_CASE("remainder carries between frames") {
    GameClock c(1000); c.reset(0);
    CHECK(c.ticksDue(75) == 1); CHECK(c.ticksDue(100) == 1); CHECK(c.totalTicks() == 2);
}
TEST_CASE("backlog beyond cap is discarded") {
    GameClock c(1000); c.reset(0);
    CHECK(c.ticksDue(1000) == 4); CHECK(c.ticksDue(1050) == 1);
}
TEST_CASE("timer wrap counts correctly") {
    GameClock c(1000); c.reset(0xFFFFFFF0u);
    CHECK(c.ticksDue(0x00000022u) == 1);
}
TEST_CASE("32768 Hz source") {
    GameClock c(32768); c.reset(0);
    CHECK(c.ticksDue(1638) == 0); CHECK(c.ticksDue(1639) == 1);
}
