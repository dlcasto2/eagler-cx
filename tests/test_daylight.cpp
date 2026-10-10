#include "doctest.h"
#include "daylight.h"

TEST_CASE("full daylight through the day, darkest at midnight") {
    CHECK(daylightFactor(1000) == 256);
    CHECK(daylightFactor(6000) == 256);
    CHECK(daylightFactor(18000) == 0);
    CHECK(skyBrightnessLevel(6000) == 7);
    CHECK(skyBrightnessLevel(18000) == NIGHT_BRIGHTNESS_LEVEL);
}

TEST_CASE("dusk and dawn fade smoothly") {
    int last = 257;
    for(uint32_t t = 12000; t <= 13800; t += 100)
    {
        const int d = daylightFactor(t);
        CHECK(d <= last);
        last = d;
    }
    CHECK(daylightFactor(13800) == 0);
    last = -1;
    for(uint32_t t = 22200; t < 24000; t += 100)
    {
        const int d = daylightFactor(t);
        CHECK(d >= last);
        last = d;
    }
    CHECK(daylightFactor(0) == 256);
}

TEST_CASE("brightness never drops below the night floor") {
    for(uint32_t t = 0; t < 24000; t += 50)
    {
        CHECK(skyBrightnessLevel(t) >= NIGHT_BRIGHTNESS_LEVEL);
        CHECK(skyBrightnessLevel(t) <= 7);
    }
}

TEST_CASE("the sky is Crafti's blue at noon and dark at night") {
    unsigned r, g, b;
    skyColor(6000, r, g, b);
    CHECK(r == 102); CHECK(g == 153); CHECK(b == 204);
    skyColor(18000, r, g, b);
    CHECK(b < 60); CHECK(r < 30);
    skyColor(12900, r, g, b);   // mid-sunset: warm
    CHECK(r > b);
}

TEST_CASE("the sun rises at 23200, peaks at 6000 and sets at 12800") {
    CHECK(sunAngleDegrees(23200) == 0);
    CHECK(sunAngleDegrees(6000) == 90);
    CHECK(sunAngleDegrees(12800) == 180);
    CHECK(sunAngleDegrees(18000) == 270);
    CHECK(sunAngleDegrees(23199) == 359);
}

TEST_CASE("the sun is still up while dusk begins, so it sets in a coloured sky") {
    CHECK(sunAngleDegrees(12000) < 180);
    CHECK(daylightFactor(12800) < 256);
    CHECK(daylightFactor(12800) > 0);
}

TEST_CASE("mid-sunset is clearly orange") {
    unsigned r, g, b;
    skyColor(12900, r, g, b);
    CHECK(r > 180); CHECK(r > g + 40); CHECK(r > b + 80);
}
