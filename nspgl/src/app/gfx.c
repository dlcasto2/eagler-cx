/* 2D drawing helpers for the RGB565 screen buffer */
#include "gfx.h"
#include "fontdata.h"
#include <string.h>

static uint16_t *FB;
static int cx0, cy0, cx1 = SCR_W, cy1 = SCR_H;

void gfx_set_fb(uint16_t *fb) { FB = fb; }
void gfx_clip(int x0, int y0, int x1, int y1)
{
    cx0 = x0 < 0 ? 0 : x0; cy0 = y0 < 0 ? 0 : y0;
    cx1 = x1 > SCR_W ? SCR_W : x1; cy1 = y1 > SCR_H ? SCR_H : y1;
}
void gfx_noclip(void) { cx0 = cy0 = 0; cx1 = SCR_W; cy1 = SCR_H; }

uint16_t rgb565(uint32_t rgb) { return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x1F)); }

static inline uint16_t blend565(uint16_t d, uint32_t rgb, int a /*0..256*/)
{
    int dr = (d >> 11) & 31, dg = (d >> 5) & 63, db = d & 31;
    int sr = (rgb >> 19) & 31, sg = (rgb >> 10) & 63, sb = (rgb >> 3) & 31;
    dr += ((sr - dr) * a) >> 8; dg += ((sg - dg) * a) >> 8; db += ((sb - db) * a) >> 8;
    return (uint16_t)((dr << 11) | (dg << 5) | db);
}

void gfx_fill(int x, int y, int w, int h, uint32_t rgb)
{
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    uint16_t c = rgb565(rgb);
    for (int j = y0; j < y1; j++) { uint16_t *p = FB + j * SCR_W; for (int i = x0; i < x1; i++) p[i] = c; }
}
void gfx_fill_a(int x, int y, int w, int h, uint32_t rgb, int alpha)
{
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    int a = alpha + (alpha >> 7);
    for (int j = y0; j < y1; j++) { uint16_t *p = FB + j * SCR_W; for (int i = x0; i < x1; i++) p[i] = blend565(p[i], rgb, a); }
}
void gfx_rect(int x, int y, int w, int h, uint32_t rgb)
{
    gfx_fill(x, y, w, 1, rgb); gfx_fill(x, y + h - 1, w, 1, rgb);
    gfx_fill(x, y, 1, h, rgb); gfx_fill(x + w - 1, y, 1, h, rgb);
}

/* decode one UTF-8 codepoint */
unsigned utf8_next(const char **ps, const char *end)
{
    const unsigned char *s = (const unsigned char *)*ps;
    unsigned c = *s++;
    if (c >= 0xC0 && (const char *)s < end) {
        int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
        c &= 0x3F >> n;
        while (n-- && (const char *)s < end && (*s & 0xC0) == 0x80) c = (c << 6) | (*s++ & 0x3F);
    }
    *ps = (const char *)s;
    return c;
}
static unsigned map_cp(unsigned c)
{
    if (c >= 32 && c <= 255) return c == 0xA0 ? ' ' : c;
    switch (c) {
    case 0x2018: case 0x2019: return '\''; case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: case 0x2212: return '-'; case 0x2022: return 0xB7; case 0x2026: return 0xB7;
    case 0x2190: return '<'; case 0x2192: return '>'; case 0x2191: return '^'; case 0x2193: return 'v';
    case 0x00D7: return 'x'; case 0x2264: return '<'; case 0x2265: return '>';
    case '\t': return ' ';
    }
    return '?';
}

int gfx_font_height(int f) { return g_fonts[f].height; }
int gfx_font_ascent(int f) { return g_fonts[f].ascent; }
int gfx_char_width(int f, unsigned cp) { return g_fonts[f].g[map_cp(cp) - 32].adv; }

int gfx_text_width(int f, const char *s, int n)
{
    const char *e = s + n; int w = 0;
    while (s < e) w += g_fonts[f].g[map_cp(utf8_next(&s, e)) - 32].adv;
    return w;
}

int gfx_text(int f, int x, int y, const char *s, int n, uint32_t rgb)
{
    const FFont *F = &g_fonts[f];
    const char *e = s + n;
    int sr = (rgb >> 19) & 31, sg = (rgb >> 10) & 63, sb = (rgb >> 3) & 31;
    while (s < e) {
        const FGlyph *g = &F->g[map_cp(utf8_next(&s, e)) - 32];
        if (x >= cx1) { x += g->adv; continue; }
        int gx = x + g->xo, gy = y + g->yo;
        const uint8_t *b = F->bits + g->off;
        for (int j = 0; j < g->h; j++) {
            int py = gy + j;
            if (py < cy0 || py >= cy1) continue;
            uint16_t *row = FB + py * SCR_W;
            for (int i = 0; i < g->w; i++) {
                int px = gx + i;
                if (px < cx0 || px >= cx1) continue;
                int k = j * g->w + i;
                int a = (b[k >> 1] >> ((k & 1) * 4)) & 15;
                if (!a) continue;
                if (a == 15) { row[px] = (uint16_t)((sr << 11) | (sg << 5) | sb); continue; }
                uint16_t d = row[px];
                int A = a * 17 + 1;
                int dr = (d >> 11) & 31, dg = (d >> 5) & 63, db = d & 31;
                dr += ((sr - dr) * A) >> 8; dg += ((sg - dg) * A) >> 8; db += ((sb - db) * A) >> 8;
                row[px] = (uint16_t)((dr << 11) | (dg << 5) | db);
            }
        }
        x += g->adv;
    }
    return x;
}

/* draw RGBA8 image (rows top-down unless flip) scaled into dest rect, alpha-blended */
void gfx_image(const uint32_t *px, int sw, int sh, int flip, int dx, int dy, int dw, int dh, int opaque, uint32_t bg)
{
    if (!px || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    int x0 = dx < cx0 ? cx0 : dx, y0 = dy < cy0 ? cy0 : dy;
    int x1 = dx + dw > cx1 ? cx1 : dx + dw, y1 = dy + dh > cy1 ? cy1 : dy + dh;
    if (x0 >= x1 || y0 >= y1) return;
    int stepx = (sw << 16) / dw, stepy = (sh << 16) / dh;
    (void)bg;
    static int xs[SCR_W];
    for (int x = x0; x < x1; x++) xs[x] = ((x - dx) * stepx + (stepx >> 1)) >> 16;
    for (int y = y0; y < y1; y++) {
        int sy = ((y - dy) * stepy + (stepy >> 1)) >> 16;
        if (sy >= sh) sy = sh - 1;
        if (flip) sy = sh - 1 - sy;
        const uint32_t *srow = px + (size_t)sy * sw;
        uint16_t *drow = FB + y * SCR_W;
        if (opaque) {
            for (int x = x0; x < x1; x++) {
                uint32_t c = srow[xs[x]];
                drow[x] = (uint16_t)(((c & 0xF8) << 8) | ((c >> 5) & 0x7E0) | ((c >> 19) & 0x1F));
            }
            continue;
        }
        for (int x = x0; x < x1; x++) {
            uint32_t c = srow[xs[x]];
            uint32_t a = c >> 24;
            if (a == 255) drow[x] = (uint16_t)(((c & 0xF8) << 8) | ((c >> 5) & 0x7E0) | ((c >> 19) & 0x1F));
            else if (a) {
                uint32_t rgb = ((c & 255) << 16) | (c & 0xFF00) | ((c >> 16) & 255);
                drow[x] = blend565(drow[x], rgb, a + (a >> 7));
            }
        }
    }
}

void gfx_glyph(int f, unsigned cp, int *adv, int *w, int *h, int *xo, int *yo, const uint8_t **bits)
{
    const FGlyph *g = &g_fonts[f].g[map_cp(cp) - 32];
    *adv = g->adv; *w = g->w; *h = g->h; *xo = g->xo; *yo = g->yo; *bits = g_fonts[f].bits + g->off;
}
