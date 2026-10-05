#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "../platform/platform.h"
enum { FNT_SANS, FNT_BOLD, FNT_MONO, FNT_H1, FNT_H2, FNT_SMALL, FNT_ITAL };
void gfx_set_fb(uint16_t *fb);
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_noclip(void);
uint16_t rgb565(uint32_t rgb);
void gfx_fill(int x, int y, int w, int h, uint32_t rgb);
void gfx_fill_a(int x, int y, int w, int h, uint32_t rgb, int alpha);
void gfx_rect(int x, int y, int w, int h, uint32_t rgb);
int gfx_font_height(int f);
int gfx_font_ascent(int f);
int gfx_char_width(int f, unsigned cp);
int gfx_text_width(int f, const char *s, int n);
int gfx_text(int f, int x, int y, const char *s, int n, uint32_t rgb);   /* y = top of line */
unsigned utf8_next(const char **ps, const char *end);
void gfx_glyph(int f, unsigned cp, int *adv, int *w, int *h, int *xo, int *yo, const uint8_t **bits);
void gfx_image(const uint32_t *px, int sw, int sh, int flip, int dx, int dy, int dw, int dh, int opaque, uint32_t bg);
#endif
