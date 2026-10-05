#include "platform_time.h"

#ifdef _TINSPIRE

// First SP804 timer module; its second counter starts at +0x20.
// See hackspire.org "Memory-mapped I/O ports on CX".
static volatile uint32_t *const timer_load    = reinterpret_cast<volatile uint32_t*>(0x900C0020);
static volatile uint32_t *const timer_value   = reinterpret_cast<volatile uint32_t*>(0x900C0024);
static volatile uint32_t *const timer_control = reinterpret_cast<volatile uint32_t*>(0x900C0028);
static volatile uint32_t *const rtc_seconds   = reinterpret_cast<volatile uint32_t*>(0x90090000);

static uint32_t saved_load, saved_control;

void platformTimeInit()
{
    saved_load = *timer_load;
    saved_control = *timer_control;

    *timer_control = 0;          // stop it while reprogramming
    *timer_load = 0xFFFFFFFF;
    *timer_control = 0x82;       // enabled, 32-bit, free-running, no interrupt
}

void platformTimeDeinit()
{
    *timer_control = 0;
    *timer_load = saved_load;
    *timer_control = saved_control;
}

uint32_t platformTicks()
{
    // SP804 counts down; flip it so it counts up.
    return 0xFFFFFFFF - *timer_value;
}

uint32_t platformTickRate()
{
    return 32768;
}

uint32_t platformRtcSeconds()
{
    return *rtc_seconds;
}

#else

#include <ctime>
#include <SDL/SDL.h>

void platformTimeInit() {}
void platformTimeDeinit() {}

uint32_t platformTicks()
{
    return SDL_GetTicks();
}

uint32_t platformTickRate()
{
    return 1000;
}

uint32_t platformRtcSeconds()
{
    return static_cast<uint32_t>(time(nullptr));
}

#endif
