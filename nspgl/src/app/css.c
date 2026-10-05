/* CSS value helpers: colors and lengths */
#include "css.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

static const struct { const char *n; uint32_t c; } named[] = {
    {"black",0x000000},{"white",0xFFFFFF},{"red",0xFF0000},{"lime",0x00FF00},{"green",0x008000},{"blue",0x0000FF},
    {"yellow",0xFFFF00},{"cyan",0x00FFFF},{"aqua",0x00FFFF},{"magenta",0xFF00FF},{"fuchsia",0xFF00FF},{"gray",0x808080},
    {"grey",0x808080},{"silver",0xC0C0C0},{"maroon",0x800000},{"olive",0x808000},{"purple",0x800080},{"teal",0x008080},
    {"navy",0x000080},{"orange",0xFFA500},{"pink",0xFFC0CB},{"brown",0xA52A2A},{"gold",0xFFD700},{"darkgray",0xA9A9A9},
    {"darkgrey",0xA9A9A9},{"lightgray",0xD3D3D3},{"lightgrey",0xD3D3D3},{"dimgray",0x696969},{"darkred",0x8B0000},
    {"darkgreen",0x006400},{"darkblue",0x00008B},{"lightblue",0xADD8E6},{"skyblue",0x87CEEB},{"steelblue",0x4682B4},
    {"coral",0xFF7F50},{"tomato",0xFF6347},{"salmon",0xFA8072},{"crimson",0xDC143C},{"indigo",0x4B0082},{"violet",0xEE82EE},
    {"orchid",0xDA70D6},{"plum",0xDDA0DD},{"khaki",0xF0E68C},{"beige",0xF5F5DC},{"ivory",0xFFFFF0},{"wheat",0xF5DEB3},
    {"tan",0xD2B48C},{"chocolate",0xD2691E},{"sienna",0xA0522D},{"turquoise",0x40E0D0},{"aquamarine",0x7FFFD4},
    {"lavender",0xE6E6FA},{"whitesmoke",0xF5F5F5},{"gainsboro",0xDCDCDC},{"slategray",0x708090},{"darkslategray",0x2F4F4F},
    {"midnightblue",0x191970},{"royalblue",0x4169E1},{"dodgerblue",0x1E90FF},{"deepskyblue",0x00BFFF},{"limegreen",0x32CD32},
    {"forestgreen",0x228B22},{"seagreen",0x2E8B57},{"orangered",0xFF4500},{"hotpink",0xFF69B4},{"deeppink",0xFF1493},
    {"firebrick",0xB22222},{"goldenrod",0xDAA520},{"darkorange",0xFF8C00},{"lightgreen",0x90EE90},{"lightyellow",0xFFFFE0},
    {"darkviolet",0x9400D3},{"slateblue",0x6A5ACD},{"cornflowerblue",0x6495ED},{"cadetblue",0x5F9EA0},{"snow",0xFFFAFA},
    {"linen",0xFAF0E6},{"mintcream",0xF5FFFA},{"azure",0xF0FFFF},{"aliceblue",0xF0F8FF},{"ghostwhite",0xF8F8FF},
    {"honeydew",0xF0FFF0},{"seashell",0xFFF5EE},{"lightcyan",0xE0FFFF},{"darkcyan",0x008B8B},{"darkmagenta",0x8B008B},
    {"darkolivegreen",0x556B2F},{"rebeccapurple",0x663399},{NULL,0}};

static float hue2rgb(float p, float q, float t)
{
    if (t < 0) t += 1; if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}

/* parse a CSS color; returns 1 and 0xAARRGGBB on success */
int css_color(const char *s, uint32_t *out)
{
    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;
    char buf[64]; int n = 0;
    while (s[n] && n < 63) { buf[n] = tolower((unsigned char)s[n]); n++; }
    buf[n] = 0;
    while (n && isspace((unsigned char)buf[n - 1])) buf[--n] = 0;
    if (!strcmp(buf, "transparent")) { *out = 0; return 1; }
    if (buf[0] == '#') {
        unsigned v = 0; int len = 0;
        for (const char *p = buf + 1; isxdigit((unsigned char)*p); p++, len++) v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0' : *p - 'a' + 10);
        if (len == 3) { *out = 0xFF000000u | ((v >> 8 & 15) * 0x110000) | ((v >> 4 & 15) * 0x1100) | ((v & 15) * 0x11); return 1; }
        if (len == 4) { *out = ((v & 15) * 0x11000000u) | ((v >> 12 & 15) * 0x110000) | ((v >> 8 & 15) * 0x1100) | ((v >> 4 & 15) * 0x11); return 1; }
        if (len == 6) { *out = 0xFF000000u | v; return 1; }
        if (len == 8) { *out = (v << 24) | (v >> 8); return 1; }
        return 0;
    }
    if (!strncmp(buf, "rgb", 3) || !strncmp(buf, "hsl", 3)) {
        const char *p = strchr(buf, '(');
        if (!p) return 0;
        float v[4] = { 0, 0, 0, 1 }; int k = 0;
        p++;
        while (*p && k < 4) {
            while (*p == ' ' || *p == ',' || *p == '/') p++;
            if (!*p || *p == ')') break;
            char *e; float f = strtof(p, &e);
            if (e == p) break;
            p = e;
            if (*p == '%') { f = (k < 3 && buf[0] == 'r') ? f * 2.55f : f / 100.0f; p++; }
            else if (!strncmp(p, "deg", 3)) p += 3;
            v[k++] = f;
        }
        int r, g, b;
        if (buf[0] == 'h') {
            float h = fmodf(v[0], 360) / 360.0f, sat = v[1] > 1 ? v[1] / 100 : v[1], l = v[2] > 1 ? v[2] / 100 : v[2];
            if (h < 0) h += 1;
            float q = l < 0.5f ? l * (1 + sat) : l + sat - l * sat, pp = 2 * l - q;
            r = (int)(hue2rgb(pp, q, h + 1.0f / 3) * 255 + 0.5f); g = (int)(hue2rgb(pp, q, h) * 255 + 0.5f); b = (int)(hue2rgb(pp, q, h - 1.0f / 3) * 255 + 0.5f);
        } else { r = (int)(v[0] + 0.5f); g = (int)(v[1] + 0.5f); b = (int)(v[2] + 0.5f); }
        r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
        float a = v[3] > 1 ? 1 : v[3] < 0 ? 0 : v[3];
        *out = ((uint32_t)(a * 255 + 0.5f) << 24) | (r << 16) | (g << 8) | b;
        return 1;
    }
    for (int i = 0; named[i].n; i++) if (!strcmp(buf, named[i].n)) { *out = 0xFF000000u | named[i].c; return 1; }
    return 0;
}

/* parse a length: returns px; pct_of used for %; em = 13px */
float css_len(const char *s, float pct_of, float dflt)
{
    if (!s || !*s) return dflt;
    while (isspace((unsigned char)*s)) s++;
    if (!strncmp(s, "calc(", 5)) {
        /* calc(100% - Npx) style */
        float a = css_len(s + 5, pct_of, 0);
        const char *m = strchr(s + 5, '-'), *pl = strchr(s + 5, '+');
        if (m && m[1] == ' ') return a - css_len(m + 1, pct_of, 0);
        if (pl) return a + css_len(pl + 1, pct_of, 0);
        return a;
    }
    char *e;
    float v = strtof(s, &e);
    if (e == s) return dflt;
    while (isspace((unsigned char)*e)) e++;
    if (*e == '%') return v * pct_of / 100.0f;
    if (!strncmp(e, "em", 2) || !strncmp(e, "rem", 3)) return v * 13.0f;
    if (!strncmp(e, "vw", 2)) return v * SCREEN_W_CSS / 100.0f;
    if (!strncmp(e, "vh", 2)) return v * SCREEN_H_CSS / 100.0f;
    if (!strncmp(e, "vmin", 4)) return v * SCREEN_H_CSS / 100.0f;
    if (!strncmp(e, "vmax", 4)) return v * SCREEN_W_CSS / 100.0f;
    if (!strncmp(e, "pt", 2)) return v * 4.0f / 3.0f;
    return v;
}
