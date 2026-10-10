#include "platform_time.h"

#ifdef _TINSPIRE

// First counter of the first SP804 timer module. On the CX it only counts once
// its clock gate is open (bit 11 of 0x900B0018) and the TI-specific register at
// 0x900C0080 selects the 32768 Hz source; without those it stays frozen.
// The OS's sleep() uses the second counter, so this one is ours.
// See omnimaga.org "Issues with the TI-Nspire CX timer" and hackspire.org.
static volatile uint32_t *const timer_load    = reinterpret_cast<volatile uint32_t*>(0x900C0000);
static volatile uint32_t *const timer_value   = reinterpret_cast<volatile uint32_t*>(0x900C0004);
static volatile uint32_t *const timer_control = reinterpret_cast<volatile uint32_t*>(0x900C0008);
static volatile uint32_t *const timer_clock   = reinterpret_cast<volatile uint32_t*>(0x900C0080);
static volatile uint32_t *const clock_gates   = reinterpret_cast<volatile uint32_t*>(0x900B0018);
static volatile uint32_t *const rtc_seconds   = reinterpret_cast<volatile uint32_t*>(0x90090000);

static uint32_t saved_load, saved_control, saved_clock;

void platformTimeInit()
{
    saved_load = *timer_load;
    saved_control = *timer_control;
    saved_clock = *timer_clock;

    *clock_gates &= ~(1u << 11);  // open the timer's clock gate
    *timer_clock = 0xA;           // 32768 Hz
    *timer_control = 0;           // stop it while reprogramming
    *timer_load = 0xFFFFFFFF;
    *timer_control = 0xC2;        // enabled, periodic, 32-bit, no interrupt
}

void platformTimeDeinit()
{
    *timer_control = 0;
    *timer_load = saved_load;
    *timer_clock = saved_clock;
    *timer_control = saved_control;
    // The clock gate stays open: closing it again could stop timers the OS uses.
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
