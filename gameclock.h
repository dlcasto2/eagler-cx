#ifndef GAMECLOCK_H
#define GAMECLOCK_H

#include <cstdint>

// Turns a free-running hardware counter into fixed-rate game ticks.
// The counter may wrap at 2^32; unsigned subtraction handles that.
class GameClock
{
public:
    static constexpr unsigned TICKS_PER_SECOND = 20, MAX_CATCH_UP = 4;
    // If the counter reads the same this many calls in a row it is taken
    // to be frozen, and the clock gives one tick per call until it moves.
    static constexpr unsigned STUCK_FRAMES = 30;

    explicit GameClock(uint32_t source_hz);

    // Forget any backlog and start counting from 'now'.
    void reset(uint32_t now);

    // Ticks to run for the time since the last call. Never more than
    // MAX_CATCH_UP; a larger backlog is dropped so time slips instead of
    // the game freezing to catch up.
    unsigned ticksDue(uint32_t now);

    uint32_t totalTicks() const { return total; }
    bool fallbackActive() const { return unchanged >= STUCK_FRAMES; }

private:
    uint32_t hz, last = 0, total = 0, unchanged = 0;
    uint64_t acc = 0;
};

#endif // GAMECLOCK_H
