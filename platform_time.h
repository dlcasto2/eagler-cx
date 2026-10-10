#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include <cstdint>

// A free-running counter for the game clock.
// CX: first counter of the first SP804 timer, 32768 Hz. PC: SDL ticks, 1000 Hz.
void platformTimeInit();
void platformTimeDeinit();
uint32_t platformTicks();       // wraps at 2^32
uint32_t platformTickRate();
uint32_t platformRtcSeconds();  // wall-clock seconds, for checking the rate

#endif // PLATFORM_TIME_H
