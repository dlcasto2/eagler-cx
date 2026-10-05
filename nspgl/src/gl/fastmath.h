/* Fast float math for the soft-float ARM9: single-precision polynomial
 * approximations (~1e-6 relative error), no double arithmetic. */
#ifndef FASTMATH_H
#define FASTMATH_H
#include <stdint.h>
#include <string.h>

static inline uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

static inline int fm_floor_i(float x) { int i = (int)x; return (x < (float)i) ? i - 1 : i; }
static inline float fm_floor(float x)
{
    uint32_t u = f2u(x);
    int e = (int)((u >> 23) & 255) - 127;
    if (e >= 23) return x;            /* already integral (or inf/nan) */
    if (e < 0) return (u >> 31) ? (x == 0 ? x : -1.0f) : 0.0f;
    uint32_t m = 0x007FFFFFu >> e;
    if (!(u & m)) return x;
    if (u >> 31) u += m;              /* negative: round away from zero */
    return u2f(u & ~m);
}

/* sin on [-pi/2, pi/2] */
static inline float fm_sin_core(float x)
{
    float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333310f + x2 * (-0.00019840874f + x2 * 2.7525562e-6f))));
}
static inline float fm_sin(float x)
{
    float k = x * 0.15915494309f;
    k = (float)(int)(k + (f2u(k) >> 31 ? -0.5f : 0.5f));
    x -= k * 6.28318530718f;
    if (x > 1.57079632679f) x = 3.14159265359f - x;
    else if (x < -1.57079632679f) x = -3.14159265359f - x;
    return fm_sin_core(x);
}
static inline float fm_cos(float x) { return fm_sin(x + 1.57079632679f); }

static inline float fm_exp2(float x)
{
    if (x > 127.0f) return 3.0e38f;
    if (x < -126.0f) return 0.0f;
    int i = fm_floor_i(x);
    float f = x - (float)i;           /* [0,1) */
    float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.055504109f + f * (0.0096181291f + f * 0.0013333558f))));
    return p * u2f((uint32_t)(i + 127) << 23);
}
static inline float fm_log2(float x)
{
    uint32_t u = f2u(x);
    if ((int32_t)u <= 0) return -3.0e38f;
    int e = (int)(u >> 23) - 127;
    float m = u2f((u & 0x007FFFFFu) | 0x3F800000u);   /* [1,2) */
    if (m > 1.41421356f) { m *= 0.5f; e++; }
    float t = (m - 1.0f) / (m + 1.0f), t2 = t * t;    /* log(m) = 2 atanh(t) */
    float l = 2.0f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * 0.14285714f)));
    return (float)e + l * 1.44269504f;
}
static inline float fm_exp(float x) { return fm_exp2(x * 1.44269504f); }
static inline float fm_log(float x) { return fm_log2(x) * 0.69314718f; }
static inline float fm_pow(float x, float y)
{
    if (x <= 0.0f) return x == 0.0f ? (y == 0.0f ? 1.0f : 0.0f) : 0.0f;
    if (y == 1.0f) return x;
    if (y == 2.0f) return x * x;
    if (y == 0.5f) { float r = u2f(0x5f3759dfu - (f2u(x) >> 1)); r = r * (1.5f - 0.5f * x * r * r); r = r * (1.5f - 0.5f * x * r * r); return x * r; }
    return fm_exp2(y * fm_log2(x));
}
static inline float fm_rsqrt(float x)
{
    float r = u2f(0x5f3759dfu - (f2u(x) >> 1));
    float h = 0.5f * x;
    r = r * (1.5f - h * r * r);
    r = r * (1.5f - h * r * r);
    r = r * (1.5f - h * r * r);
    return r;
}
static inline float fm_sqrt(float x) { return (int32_t)f2u(x) > 0 ? x * fm_rsqrt(x) : 0.0f; }

/* atan on all reals */
static inline float fm_atan(float x)
{
    int neg = f2u(x) >> 31;
    if (neg) x = -x;
    int inv = x > 1.0f;
    if (inv) x = 1.0f / x;
    /* reduce further: atan(x) = pi/6 + atan((x*sqrt3-1)/(sqrt3+x)) for x > tan(pi/12) */
    int red = x > 0.26794919f;
    if (red) x = (x * 1.73205081f - 1.0f) / (1.73205081f + x);
    float x2 = x * x;
    float r = x * (1.0f + x2 * (-0.33333333f + x2 * (0.2f + x2 * (-0.14285714f + x2 * 0.11111111f))));
    if (red) r += 0.52359878f;
    if (inv) r = 1.57079632679f - r;
    return neg ? -r : r;
}
static inline float fm_atan2(float y, float x)
{
    if (x == 0.0f) return y > 0.0f ? 1.57079632679f : (y < 0.0f ? -1.57079632679f : 0.0f);
    float a = fm_atan(y / x);
    if (x < 0.0f) a += (f2u(y) >> 31) ? -3.14159265359f : 3.14159265359f;
    return a;
}
static inline float fm_asin(float x)
{
    if (x >= 1.0f) return 1.57079632679f;
    if (x <= -1.0f) return -1.57079632679f;
    return fm_atan(x * fm_rsqrt(1.0f - x * x));
}
static inline float fm_acos(float x) { return 1.57079632679f - fm_asin(x); }
static inline float fm_tan(float x)
{
    float c = fm_cos(x);
    return fm_sin(x) / (c == 0.0f ? 1e-30f : c);
}
#endif
