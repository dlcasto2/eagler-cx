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
TEST_CASE("a frozen counter falls back to one tick per frame") {
    GameClock c(32768); c.reset(500);
    unsigned ticks = 0;
    for(unsigned i = 0; i < GameClock::STUCK_FRAMES; ++i)
        ticks += c.ticksDue(500);
    CHECK(ticks == 0);
    CHECK(c.fallbackActive());
    CHECK(c.ticksDue(500) == 1);
    CHECK(c.ticksDue(500) == 1);
}
TEST_CASE("the fallback ends once the counter moves again") {
    GameClock c(1000); c.reset(0);
    for(unsigned i = 0; i < GameClock::STUCK_FRAMES + 3; ++i)
        c.ticksDue(0);
    CHECK(c.fallbackActive());
    CHECK(c.ticksDue(50) == 1);
    CHECK_FALSE(c.fallbackActive());
    CHECK(c.ticksDue(60) == 0);
}
