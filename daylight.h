#ifndef DAYLIGHT_H
#define DAYLIGHT_H

#include <cstdint>

// The day/night cycle, as 1.8.8 times it: 24000 ticks a day (20 minutes),
// sunrise at 0, noon at 6000, sunset at 12000, midnight at 18000.

constexpr uint32_t DAY_TICKS = 24000;
constexpr int NIGHT_BRIGHTNESS_LEVEL = 3;   // nights are dim, not black

// 256 in full daylight, 0 at night, fading over dusk (12000-13800)
// and dawn (22200-24000).
int daylightFactor(uint32_t time_of_day);

// The world brightness level (lighting.h) for this time.
int skyBrightnessLevel(uint32_t time_of_day);

// The sky colour (0-255 per channel): Crafti's blue by day, warm at
// sunrise and sunset, dark blue at night.
void skyColor(uint32_t time_of_day, unsigned &r, unsigned &g, unsigned &b);

// Where the sun is along its circle, 0-359: 0 rising (23200), 90 overhead
// (6000), 180 setting (12800).
// The moon is opposite.
int sunAngleDegrees(uint32_t time_of_day);

#endif // DAYLIGHT_H
