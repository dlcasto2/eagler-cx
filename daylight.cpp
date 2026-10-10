#include "daylight.h"

namespace {

constexpr uint32_t DUSK_START = 12000, DUSK_END = 13800, DAWN_START = 22200;

unsigned mix(unsigned a, unsigned b, int t256)
{
    return (a * static_cast<unsigned>(256 - t256) + b * static_cast<unsigned>(t256)) / 256;
}

} // namespace

int daylightFactor(uint32_t time_of_day)
{
    const uint32_t t = time_of_day % DAY_TICKS;
    if(t < DUSK_START)
        return 256;
    if(t < DUSK_END)
        return static_cast<int>(256 - (t - DUSK_START) * 256 / (DUSK_END - DUSK_START));
    if(t < DAWN_START)
        return 0;
    return static_cast<int>((t - DAWN_START) * 256 / (DAY_TICKS - DAWN_START));
}

int skyBrightnessLevel(uint32_t time_of_day)
{
    // NIGHT_BRIGHTNESS_LEVEL..7, rounded to the nearest level
    return NIGHT_BRIGHTNESS_LEVEL + (daylightFactor(time_of_day) * (7 - NIGHT_BRIGHTNESS_LEVEL) + 128) / 256;
}

void skyColor(uint32_t time_of_day, unsigned &r, unsigned &g, unsigned &b)
{
    const int d = daylightFactor(time_of_day);
    r = mix(10, 102, d);
    g = mix(14, 153, d);
    b = mix(40, 204, d);

    // Halfway through dusk or dawn the sky glows orange.
    const int half = d < 128 ? d : 256 - d;   // 0 at full day/night, 128 at the middle
    const int glow = half * 2 - half * half / 128 / 2;   // a soft peak, 192 at the middle
    if(glow > 0)
    {
        r = mix(r, 236, glow);
        g = mix(g, 120, glow);
        b = mix(b, 64, glow);
    }
}

int sunAngleDegrees(uint32_t time_of_day)
{
    // As in 1.8.8 the sun is up a little longer than it is down: it rises at
    // 23200 and sets at 12800, so it sinks into a coloured dusk sky.
    constexpr uint32_t RISE = 23200, SET = 12800, DAY_ARC = DAY_TICKS - RISE + SET;
    const uint32_t since_rise = (time_of_day % DAY_TICKS + DAY_TICKS - RISE) % DAY_TICKS;
    if(since_rise < DAY_ARC)
        return static_cast<int>(since_rise * 180 / DAY_ARC);
    return 180 + static_cast<int>((since_rise - DAY_ARC) * 180 / (DAY_TICKS - DAY_ARC));
}
