#include "gameclock.h"

constexpr unsigned GameClock::TICKS_PER_SECOND, GameClock::MAX_CATCH_UP;

GameClock::GameClock(uint32_t source_hz) : hz(source_hz)
{
}

void GameClock::reset(uint32_t now)
{
    last = now;
    acc = 0;
}

unsigned GameClock::ticksDue(uint32_t now)
{
    const uint32_t elapsed = now - last;
    last = now;

    acc += static_cast<uint64_t>(elapsed) * TICKS_PER_SECOND;
    uint64_t due = acc / hz;
    acc %= hz;

    if(due > MAX_CATCH_UP)
    {
        due = MAX_CATCH_UP;
        acc = 0;
    }

    total += static_cast<uint32_t>(due);
    return static_cast<unsigned>(due);
}
