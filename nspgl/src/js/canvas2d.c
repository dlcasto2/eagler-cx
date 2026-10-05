/* Minimal CanvasRenderingContext2D (software, RGBA8, nonzero scanline fill) */
#include "canvas2d.h"
#include "webgl_js.h"
#include "../app/css.h"
#include "../app/gfx.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

static JSClassID c2d_cid;
static JSValue c2d_proto;

typedef struct St {
    uint32_t fill, stroke;     /* ARGB */
    float lw, alpha;
    float m[6];                /* a b c d e f */
    int font, align, baseline; float fsize;
    int comp;                  /* 0 source-over, 1 copy, 2 lighter, 3 destination-out */
} St;

typedef struct C2D {
    int vw, vh, w, h;          /* virtual canvas size, real buffer */
    float sc;                  /* real/virtual */
    uint32_t *px;              /* ABGR (RGBA bytes), top-down */
    St st, stack[32]; int sp;
    float *pts; int npts, cappts;     /* flattened path points (device coords) */
    int *subs; int nsubs, capsubs;    /* subpath start indices */
    int closed_flag[256];
    float cx, cy;              /* current point (user space) */
} C2D;

static JSValue new_ta(JSContext *ctx, JSValue ab, JSTypedArrayEnum t)
{
    JSValueConst args[3] = { ab, JS_UNDEFINED, JS_UNDEFINED };
    return JS_NewTypedArray(ctx, 3, args, t);
}
#define FN(name) static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
#define A(i) (i < argc ? argv[i] : JS_UNDEFINED)
static double ad(JSContext *ctx, JSValueConst v) { double r = 0; JS_ToFloat64(ctx, &r, v); return r; }
#define D(i) ((float)ad(ctx, A(i)))
#define C C2D *c = JS_GetOpaque2(ctx, this_val, c2d_cid); if (!c) return JS_EXCEPTION;

static void xf(C2D *c, float x, float y, float *ox, float *oy)
{
    const float *m = c->st.m;
    *ox = (m[0] * x + m[2] * y + m[4]) * c->sc;
    *oy = (m[1] * x + m[3] * y + m[5]) * c->sc;
}

/* ---------------- pixel ops ---------------- */
static inline void plot(C2D *c, int x, int y, uint32_t argb, int cov /*0..256*/)
{
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h) return;
    uint32_t *d = &c->px[(size_t)y * c->w + x];
    int sa = (int)(((argb >> 24) * c->st.alpha) * cov) >> 8;
    int sr = (argb >> 16) & 255, sg = (argb >> 8) & 255, sb = argb & 255;
    if (c->st.comp == 1) { *d = (uint32_t)sr | (sg << 8) | (sb << 16) | ((uint32_t)sa << 24); return; }
    if (c->st.comp == 3) { int da = *d >> 24; da = da * (255 - sa) / 255; *d = (*d & 0xFFFFFF) | ((uint32_t)da << 24); return; }
    if (sa <= 0) return;
    uint32_t dp = *d;
    int dr = dp & 255, dg = (dp >> 8) & 255, db = (dp >> 16) & 255, da = dp >> 24;
    if (c->st.comp == 2) {
        dr += sr * sa / 255; dg += sg * sa / 255; db += sb * sa / 255; da += sa;
        if (dr > 255) dr = 255; if (dg > 255) dg = 255; if (db > 255) db = 255; if (da > 255) da = 255;
    } else if (sa >= 255) { dr = sr; dg = sg; db = sb; da = 255; }
    else {
        int oa = sa + da * (255 - sa) / 255;
        if (oa) {
            dr = (sr * sa + dr * da * (255 - sa) / 255) / oa;
            dg = (sg * sa + dg * da * (255 - sa) / 255) / oa;
            db = (sb * sa + db * da * (255 - sa) / 255) / oa;
        }
        da = oa;
    }
    *d = (uint32_t)dr | (dg << 8) | (db << 16) | ((uint32_t)da << 24);
}

/* ---------------- path ---------------- */
static void addpt(C2D *c, float x, float y)
{
    if (c->npts * 2 + 2 > c->cappts) { c->cappts = c->cappts ? c->cappts * 2 : 256; c->pts = realloc(c->pts, c->cappts * sizeof(float)); }
    float dx, dy; xf(c, x, y, &dx, &dy);
    c->pts[c->npts * 2] = dx; c->pts[c->npts * 2 + 1] = dy; c->npts++;
    c->cx = x; c->cy = y;
}
static void newsub(C2D *c)
{
    if (c->nsubs == c->capsubs) { c->capsubs = c->capsubs ? c->capsubs * 2 : 16; c->subs = realloc(c->subs, c->capsubs * sizeof(int)); }
    if (c->nsubs < 256) c->closed_flag[c->nsubs] = 0;
    c->subs[c->nsubs++] = c->npts;
}
static int sub_end(C2D *c, int i) { return i + 1 < c->nsubs ? c->subs[i + 1] : c->npts; }

FN(m_beginPath) { C (void)argc; (void)argv; c->npts = 0; c->nsubs = 0; return JS_UNDEFINED; }
FN(m_moveTo) { C newsub(c); addpt(c, D(0), D(1)); return JS_UNDEFINED; }
FN(m_lineTo) { C if (!c->nsubs) newsub(c); addpt(c, D(0), D(1)); return JS_UNDEFINED; }
FN(m_closePath)
{
    C (void)argc; (void)argv;
    if (c->nsubs) {
        int s = c->subs[c->nsubs - 1];
        if (c->nsubs - 1 < 256) c->closed_flag[c->nsubs - 1] = 1;
        if (c->npts > s) {   /* restart at first point */
            float fx = c->pts[s * 2], fy = c->pts[s * 2 + 1];
            /* invert transform not needed: keep device coords */
            newsub(c);
            if (c->npts * 2 + 2 > c->cappts) { c->cappts *= 2; c->pts = realloc(c->pts, c->cappts * sizeof(float)); }
            c->pts[c->npts * 2] = fx; c->pts[c->npts * 2 + 1] = fy; c->npts++;
        }
    }
    return JS_UNDEFINED;
}
FN(m_rect)
{
    C float x = D(0), y = D(1), w = D(2), h = D(3);
    newsub(c); addpt(c, x, y); addpt(c, x + w, y); addpt(c, x + w, y + h); addpt(c, x, y + h); addpt(c, x, y);
    if (c->nsubs - 1 < 256) c->closed_flag[c->nsubs - 1] = 1;
    newsub(c); addpt(c, x, y);
    return JS_UNDEFINED;
}
static int segs_for(C2D *c, float r, float ang)
{
    float s = r * c->sc * sqrtf(fabsf(c->st.m[0] * c->st.m[3] - c->st.m[1] * c->st.m[2]));
    int n = (int)(fabsf(ang) * sqrtf(s > 1 ? s : 1) * 1.2f) + 2;
    return n > 128 ? 128 : n;
}
static void arc_pts(C2D *c, float x, float y, float rx, float ry, float rot, float a0, float a1, int ccw)
{
    float tau = 6.2831853f;
    if (!ccw && a1 - a0 >= tau) a1 = a0 + tau;
    else if (ccw && a0 - a1 >= tau) a1 = a0 - tau;
    else {
        if (!ccw) { while (a1 < a0) a1 += tau; }
        else { while (a1 > a0) a1 -= tau; }
    }
    int n = segs_for(c, rx > ry ? rx : ry, a1 - a0);
    if (!c->nsubs) newsub(c);
    float cr = cosf(rot), sr = sinf(rot);
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1 - a0) * i / n;
        float px = rx * cosf(a), py = ry * sinf(a);
        addpt(c, x + px * cr - py * sr, y + px * sr + py * cr);
    }
}
FN(m_arc) { C arc_pts(c, D(0), D(1), D(2), D(2), 0, D(3), D(4), JS_ToBool(ctx, A(5))); return JS_UNDEFINED; }
FN(m_ellipse) { C arc_pts(c, D(0), D(1), D(2), D(3), D(4), D(5), D(6), JS_ToBool(ctx, A(7))); return JS_UNDEFINED; }
FN(m_arcTo) { C if (!c->nsubs) newsub(c); addpt(c, D(0), D(1)); addpt(c, D(2), D(3)); return JS_UNDEFINED; }
FN(m_quadTo)
{
    C float x0 = c->cx, y0 = c->cy, qx = D(0), qy = D(1), x = D(2), y = D(3);
    if (!c->nsubs) newsub(c);
    for (int i = 1; i <= 12; i++) { float t = i / 12.0f, u = 1 - t; addpt(c, u * u * x0 + 2 * u * t * qx + t * t * x, u * u * y0 + 2 * u * t * qy + t * t * y); }
    return JS_UNDEFINED;
}
FN(m_bezierTo)
{
    C float x0 = c->cx, y0 = c->cy, ax = D(0), ay = D(1), bx = D(2), by = D(3), x = D(4), y = D(5);
    if (!c->nsubs) newsub(c);
    for (int i = 1; i <= 16; i++) {
        float t = i / 16.0f, u = 1 - t;
        addpt(c, u * u * u * x0 + 3 * u * u * t * ax + 3 * u * t * t * bx + t * t * t * x, u * u * u * y0 + 3 * u * u * t * ay + 3 * u * t * t * by + t * t * t * y);
    }
    return JS_UNDEFINED;
}

/* nonzero/evenodd scanline polygon fill of the current device-space polygon set */
static void fill_polys(C2D *c, const float *pts, const int *subs, int nsubs, int npts, uint32_t col, int evenodd)
{
    if (npts < 2) return;
    float miny = 1e30f, maxy = -1e30f;
    for (int i = 0; i < npts; i++) { float y = pts[i * 2 + 1]; if (y < miny) miny = y; if (y > maxy) maxy = y; }
    int y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
    if (y0 < 0) y0 = 0;
    if (y1 > c->h) y1 = c->h;
    float *xs = malloc(sizeof(float) * (npts + 8)); int *ws = malloc(sizeof(int) * (npts + 8));
    for (int y = y0; y < y1; y++) {
        float sy = y + 0.5f; int n = 0;
        for (int s = 0; s < nsubs; s++) {
            int a = subs[s], b = s + 1 < nsubs ? subs[s + 1] : npts;
            if (b - a < 2) continue;
            for (int i = a; i < b; i++) {
                int j = (i + 1 < b) ? i + 1 : a;    /* implicit close */
                float ax = pts[i * 2], ay = pts[i * 2 + 1], bx = pts[j * 2], by = pts[j * 2 + 1];
                if ((ay <= sy) == (by <= sy)) continue;
                float t = (sy - ay) / (by - ay);
                xs[n] = ax + (bx - ax) * t; ws[n] = by > ay ? 1 : -1; n++;
            }
        }
        /* sort */
        for (int i = 1; i < n; i++) { float x = xs[i]; int w = ws[i], k = i - 1; while (k >= 0 && xs[k] > x) { xs[k + 1] = xs[k]; ws[k + 1] = ws[k]; k--; } xs[k + 1] = x; ws[k + 1] = w; }
        int wind = 0;
        for (int i = 0; i + 1 < n; i++) {
            wind += evenodd ? 1 : ws[i];
            int inside = evenodd ? (wind & 1) : wind != 0;
            if (!inside) continue;
            int xa = (int)ceilf(xs[i] - 0.5f), xb = (int)ceilf(xs[i + 1] - 0.5f);
            if (xa < 0) xa = 0;
            if (xb > c->w) xb = c->w;
            for (int x = xa; x < xb; x++) plot(c, x, y, col, 256);
        }
    }
    free(xs); free(ws);
}

FN(m_fill)
{
    C int eo = 0;
    for (int i = 0; i < argc; i++) if (JS_IsString(argv[i])) { const char *s = JS_ToCString(ctx, argv[i]); eo = s && !strcmp(s, "evenodd"); JS_FreeCString(ctx, s); }
    fill_polys(c, c->pts, c->subs, c->nsubs, c->npts, c->st.fill, eo);
    return JS_UNDEFINED;
}

static void thick_line(C2D *c, float ax, float ay, float bx, float by, float w, uint32_t col)
{
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    if (w <= 1.5f) {   /* thin: DDA */
        int n = (int)(fmaxf(fabsf(dx), fabsf(dy))) + 1;
        for (int i = 0; i <= n; i++) { float t = (float)i / n; plot(c, (int)floorf(ax + dx * t), (int)floorf(ay + dy * t), col, 256); }
        return;
    }
    if (len < 1e-6f) return;
    float nx = -dy / len * w * 0.5f, ny = dx / len * w * 0.5f;
    float q[10] = { ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, ax - nx, ay - ny, ax + nx, ay + ny };
    int s0 = 0;
    fill_polys(c, q, &s0, 1, 5, col, 0);
}
FN(m_stroke)
{
    C (void)argc; (void)argv;
    float w = c->st.lw * c->sc * sqrtf(fabsf(c->st.m[0] * c->st.m[3] - c->st.m[1] * c->st.m[2]));
    for (int s = 0; s < c->nsubs; s++) {
        int a = c->subs[s], b = sub_end(c, s);
        for (int i = a; i + 1 < b; i++)
            thick_line(c, c->pts[i * 2], c->pts[i * 2 + 1], c->pts[i * 2 + 2], c->pts[i * 2 + 3], w, c->st.stroke);
        if (s < 256 && c->closed_flag[s] && b - a > 2)
            thick_line(c, c->pts[(b - 1) * 2], c->pts[(b - 1) * 2 + 1], c->pts[a * 2], c->pts[a * 2 + 1], w, c->st.stroke);
    }
    return JS_UNDEFINED;
}

static void rect_poly(C2D *c, float x, float y, float w, float h, float *q)
{
    xf(c, x, y, &q[0], &q[1]); xf(c, x + w, y, &q[2], &q[3]); xf(c, x + w, y + h, &q[4], &q[5]); xf(c, x, y + h, &q[6], &q[7]);
}
FN(m_fillRect)
{
    C float q[8]; rect_poly(c, D(0), D(1), D(2), D(3), q);
    int s0 = 0; fill_polys(c, q, &s0, 1, 4, c->st.fill, 0);
    return JS_UNDEFINED;
}
FN(m_clearRect)
{
    C float q[8]; rect_poly(c, D(0), D(1), D(2), D(3), q);
    int comp = c->st.comp; float al = c->st.alpha;
    c->st.comp = 1; c->st.alpha = 1;
    int s0 = 0; fill_polys(c, q, &s0, 1, 4, 0, 0);
    c->st.comp = comp; c->st.alpha = al;
    return JS_UNDEFINED;
}
FN(m_strokeRect)
{
    C float q[8]; rect_poly(c, D(0), D(1), D(2), D(3), q);
    float w = c->st.lw * c->sc;
    for (int i = 0; i < 4; i++) { int j = (i + 1) & 3; thick_line(c, q[i * 2], q[i * 2 + 1], q[j * 2], q[j * 2 + 1], w, c->st.stroke); }
    return JS_UNDEFINED;
}

/* ---------------- styles ---------------- */
static int style_color(JSContext *ctx, JSValueConst v, uint32_t *out)
{
    if (JS_IsString(v)) {
        const char *s = JS_ToCString(ctx, v);
        int ok = css_color(s, out);
        JS_FreeCString(ctx, s);
        return ok;
    }
    if (JS_IsObject(v)) {   /* gradient/pattern object: use its _color */
        JSValue cv = JS_GetPropertyStr(ctx, v, "_color");
        int ok = 0;
        if (JS_IsString(cv)) { const char *s = JS_ToCString(ctx, cv); ok = css_color(s, out); JS_FreeCString(ctx, s); }
        JS_FreeValue(ctx, cv);
        return ok;
    }
    return 0;
}
static JSValue color_str(JSContext *ctx, uint32_t c)
{
    char b[32];
    if ((c >> 24) == 255) snprintf(b, sizeof b, "#%06x", c & 0xFFFFFF);
    else snprintf(b, sizeof b, "rgba(%d, %d, %d, %g)", (c >> 16) & 255, (c >> 8) & 255, c & 255, (c >> 24) / 255.0);
    return JS_NewString(ctx, b);
}
static JSValue g_fillStyle(JSContext *ctx, JSValueConst this_val) { C return color_str(ctx, c->st.fill); }
static JSValue s_fillStyle(JSContext *ctx, JSValueConst this_val, JSValueConst v) { C style_color(ctx, v, &c->st.fill); return JS_UNDEFINED; }
static JSValue g_strokeStyle(JSContext *ctx, JSValueConst this_val) { C return color_str(ctx, c->st.stroke); }
static JSValue s_strokeStyle(JSContext *ctx, JSValueConst this_val, JSValueConst v) { C style_color(ctx, v, &c->st.stroke); return JS_UNDEFINED; }
static JSValue g_lineWidth(JSContext *ctx, JSValueConst this_val) { C return JS_NewFloat64(ctx, c->st.lw); }
static JSValue s_lineWidth(JSContext *ctx, JSValueConst this_val, JSValueConst v) { C double d = ad(ctx, v); if (d > 0) c->st.lw = (float)d; return JS_UNDEFINED; }
static JSValue g_alpha(JSContext *ctx, JSValueConst this_val) { C return JS_NewFloat64(ctx, c->st.alpha); }
static JSValue s_alpha(JSContext *ctx, JSValueConst this_val, JSValueConst v) { C double d = ad(ctx, v); if (d >= 0 && d <= 1) c->st.alpha = (float)d; return JS_UNDEFINED; }
static JSValue g_comp(JSContext *ctx, JSValueConst this_val)
{
    C static const char *n[] = { "source-over", "copy", "lighter", "destination-out" };
    return JS_NewString(ctx, n[c->st.comp & 3]);
}
static JSValue s_comp(JSContext *ctx, JSValueConst this_val, JSValueConst v)
{
    C const char *s = JS_ToCString(ctx, v);
    if (s) c->st.comp = !strcmp(s, "copy") ? 1 : !strcmp(s, "lighter") ? 2 : !strcmp(s, "destination-out") ? 3 : 0;
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
static JSValue g_font(JSContext *ctx, JSValueConst this_val)
{
    C char b[48]; snprintf(b, sizeof b, "%s%dpx sans-serif", c->st.font == FNT_BOLD || c->st.font == FNT_H1 || c->st.font == FNT_H2 ? "bold " : "", (int)c->st.fsize);
    return JS_NewString(ctx, b);
}
static JSValue s_font(JSContext *ctx, JSValueConst this_val, JSValueConst v)
{
    C const char *s = JS_ToCString(ctx, v);
    if (s) {
        int bold = strstr(s, "bold") != NULL || strstr(s, "700") != NULL || strstr(s, "800") != NULL || strstr(s, "900") != NULL;
        int mono = strstr(s, "mono") != NULL || strstr(s, "Courier") != NULL;
        float sz = 10;
        for (const char *p = s; *p; p++) if (*p >= '0' && *p <= '9') { char *e; float f = strtof(p, &e); if (!strncmp(e, "px", 2) || !strncmp(e, "pt", 2) || !strncmp(e, "em", 2)) { sz = !strncmp(e, "pt", 2) ? f * 4 / 3 : !strncmp(e, "em", 2) ? f * 13 : f; break; } p = e - 1; }
        c->st.fsize = sz;
        float dsz = sz * c->sc;
        c->st.font = dsz < 9 ? FNT_SMALL : dsz < 13 ? (mono ? FNT_MONO : bold ? FNT_BOLD : FNT_SANS) : dsz < 17 ? FNT_H2 : FNT_H1;
    }
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
static JSValue g_align(JSContext *ctx, JSValueConst this_val) { C static const char *n[] = { "start", "center", "end" }; return JS_NewString(ctx, n[c->st.align]); }
static JSValue s_align(JSContext *ctx, JSValueConst this_val, JSValueConst v)
{
    C const char *s = JS_ToCString(ctx, v);
    if (s) c->st.align = !strcmp(s, "center") ? 1 : (!strcmp(s, "right") || !strcmp(s, "end")) ? 2 : 0;
    JS_FreeCString(ctx, s); return JS_UNDEFINED;
}
static JSValue g_base(JSContext *ctx, JSValueConst this_val) { C static const char *n[] = { "alphabetic", "top", "middle", "bottom" }; return JS_NewString(ctx, n[c->st.baseline]); }
static JSValue s_base(JSContext *ctx, JSValueConst this_val, JSValueConst v)
{
    C const char *s = JS_ToCString(ctx, v);
    if (s) c->st.baseline = (!strcmp(s, "top") || !strcmp(s, "hanging")) ? 1 : !strcmp(s, "middle") ? 2 : !strcmp(s, "bottom") || !strcmp(s, "ideographic") ? 3 : 0;
    JS_FreeCString(ctx, s); return JS_UNDEFINED;
}

/* ---------------- text ---------------- */
static int text_w(int f, const char *s) { return gfx_text_width(f, s, strlen(s)); }
static void draw_text(C2D *c, const char *s, float x, float y, uint32_t col)
{
    int f = c->st.font;
    float dx, dy; xf(c, x, y, &dx, &dy);
    int tw = text_w(f, s);
    if (c->st.align == 1) dx -= tw / 2; else if (c->st.align == 2) dx -= tw;
    int asc = gfx_font_ascent(f), hgt = gfx_font_height(f);
    float top = c->st.baseline == 1 ? dy : c->st.baseline == 2 ? dy - hgt / 2 : c->st.baseline == 3 ? dy - hgt : dy - asc;
    int px = (int)floorf(dx + 0.5f), py = (int)floorf(top + 0.5f);
    const char *e = s + strlen(s);
    while (s < e) {
        unsigned cp = utf8_next(&s, e);
        int adv, w, h, xo, yo; const uint8_t *b;
        gfx_glyph(f, cp, &adv, &w, &h, &xo, &yo, &b);
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
            int k = j * w + i, a = (b[k >> 1] >> ((k & 1) * 4)) & 15;
            if (a) plot(c, px + xo + i, py + yo + j, col, a * 17 + 1);
        }
        px += adv;
    }
}
FN(m_fillText) { C const char *s = JS_ToCString(ctx, A(0)); if (s) draw_text(c, s, D(1), D(2), c->st.fill); JS_FreeCString(ctx, s); return JS_UNDEFINED; }
FN(m_strokeText) { C const char *s = JS_ToCString(ctx, A(0)); if (s) draw_text(c, s, D(1), D(2), c->st.stroke); JS_FreeCString(ctx, s); return JS_UNDEFINED; }
FN(m_measureText)
{
    C const char *s = JS_ToCString(ctx, A(0));
    float w = s ? text_w(c->st.font, s) / c->sc : 0;
    JS_FreeCString(ctx, s);
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, o, "actualBoundingBoxAscent", JS_NewFloat64(ctx, c->st.fsize * 0.8));
    JS_SetPropertyStr(ctx, o, "actualBoundingBoxDescent", JS_NewFloat64(ctx, c->st.fsize * 0.2));
    return o;
}

/* ---------------- transforms / state ---------------- */
static void mul(float *m, float a, float b, float cc, float d, float e, float f)
{
    float r[6] = { m[0] * a + m[2] * b, m[1] * a + m[3] * b, m[0] * cc + m[2] * d, m[1] * cc + m[3] * d,
                   m[0] * e + m[2] * f + m[4], m[1] * e + m[3] * f + m[5] };
    memcpy(m, r, sizeof r);
}
FN(m_translate) { C mul(c->st.m, 1, 0, 0, 1, D(0), D(1)); return JS_UNDEFINED; }
FN(m_scale) { C mul(c->st.m, D(0), 0, 0, D(1), 0, 0); return JS_UNDEFINED; }
FN(m_rotate) { C float a = D(0), cs = cosf(a), sn = sinf(a); mul(c->st.m, cs, sn, -sn, cs, 0, 0); return JS_UNDEFINED; }
FN(m_transform) { C mul(c->st.m, D(0), D(1), D(2), D(3), D(4), D(5)); return JS_UNDEFINED; }
FN(m_setTransform)
{
    C float *m = c->st.m;
    if (argc >= 6) { m[0] = D(0); m[1] = D(1); m[2] = D(2); m[3] = D(3); m[4] = D(4); m[5] = D(5); }
    else { m[0] = 1; m[1] = 0; m[2] = 0; m[3] = 1; m[4] = 0; m[5] = 0; }
    return JS_UNDEFINED;
}
FN(m_save) { C (void)argc; (void)argv; if (c->sp < 32) c->stack[c->sp++] = c->st; return JS_UNDEFINED; }
FN(m_restore) { C (void)argc; (void)argv; if (c->sp > 0) c->st = c->stack[--c->sp]; return JS_UNDEFINED; }

/* ---------------- images ---------------- */
static JSValue mk_imagedata(JSContext *ctx, int w, int h, const uint8_t *src)
{
    JSValue o = JS_NewObject(ctx);
    size_t n = (size_t)w * h * 4;
    uint8_t *buf = calloc(n ? n : 1, 1);
    if (src) memcpy(buf, src, n);
    JSValue ab = JS_NewArrayBufferCopy(ctx, buf, n);
    free(buf);
    JSValue arr = new_ta(ctx, ab, JS_TYPED_ARRAY_UINT8C);
    JS_FreeValue(ctx, ab);
    JS_SetPropertyStr(ctx, o, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, o, "height", JS_NewInt32(ctx, h));
    JS_SetPropertyStr(ctx, o, "data", arr);
    return o;
}
FN(m_createImageData)
{
    (void)this_val;
    int w, h;
    if (JS_IsObject(A(0))) { int32_t t; JS_ToInt32(ctx, &t, JS_GetPropertyStr(ctx, A(0), "width")); w = t; JS_ToInt32(ctx, &t, JS_GetPropertyStr(ctx, A(0), "height")); h = t; }
    else { w = (int)D(0); h = (int)D(1); }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return mk_imagedata(ctx, w, h, NULL);
}
FN(m_getImageData)
{
    C int x = (int)D(0), y = (int)D(1), w = (int)D(2), h = (int)D(3);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    uint8_t *buf = calloc((size_t)w * h, 4);
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int sx = (int)((x + i + 0.5f) * c->sc), sy = (int)((y + j + 0.5f) * c->sc);
        if (sx >= 0 && sy >= 0 && sx < c->w && sy < c->h) memcpy(buf + ((size_t)j * w + i) * 4, &c->px[(size_t)sy * c->w + sx], 4);
    }
    JSValue r = mk_imagedata(ctx, w, h, buf);
    free(buf);
    return r;
}
static uint8_t *bytes_of(JSContext *ctx, JSValueConst v, size_t *len)
{
    size_t off, l, b;
    JSValue buf = JS_GetTypedArrayBuffer(ctx, v, &off, &l, &b);
    if (JS_IsException(buf)) { JS_FreeValue(ctx, JS_GetException(ctx)); return NULL; }
    size_t sz; uint8_t *p = JS_GetArrayBuffer(ctx, &sz, buf);
    JS_FreeValue(ctx, buf);
    *len = l;
    return p ? p + off : NULL;
}
FN(m_putImageData)
{
    C JSValueConst id = A(0);
    int32_t w = 0, h = 0;
    JS_ToInt32(ctx, &w, JS_GetPropertyStr(ctx, id, "width")); JS_ToInt32(ctx, &h, JS_GetPropertyStr(ctx, id, "height"));
    JSValue d = JS_GetPropertyStr(ctx, id, "data");
    size_t len; uint8_t *p = bytes_of(ctx, d, &len);
    int dx = (int)D(1), dy = (int)D(2);
    if (p && len >= (size_t)w * h * 4) {
        if (c->sc == 1.0f) {
            for (int j = 0; j < h; j++) {
                int y = dy + j; if (y < 0 || y >= c->h) continue;
                for (int i = 0; i < w; i++) { int x = dx + i; if (x < 0 || x >= c->w) continue; memcpy(&c->px[(size_t)y * c->w + x], p + ((size_t)j * w + i) * 4, 4); }
            }
        } else {
            int x0 = (int)(dx * c->sc), y0 = (int)(dy * c->sc), x1 = (int)((dx + w) * c->sc), y1 = (int)((dy + h) * c->sc);
            for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
                if (x < 0 || y < 0 || x >= c->w || y >= c->h) continue;
                int sx = (int)((x + 0.5f) / c->sc) - dx, sy = (int)((y + 0.5f) / c->sc) - dy;
                if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
                memcpy(&c->px[(size_t)y * c->w + x], p + ((size_t)sy * w + sx) * 4, 4);
            }
        }
    }
    JS_FreeValue(ctx, d);
    return JS_UNDEFINED;
}

/* image source pixels: image element (_px), canvas (_ctx), ImageData */
static int src_pixels(JSContext *ctx, JSValueConst s, const uint32_t **px, int *w, int *h, int *flip, uint8_t **owned)
{
    *owned = NULL; *flip = 0;
    JSValue p = JS_GetPropertyStr(ctx, s, "_px");
    if (JS_IsObject(p)) {
        size_t sz; uint8_t *b = JS_GetArrayBuffer(ctx, &sz, p);
        int32_t ww = 0, hh = 0;
        JS_ToInt32(ctx, &ww, JS_GetPropertyStr(ctx, s, "naturalWidth")); JS_ToInt32(ctx, &hh, JS_GetPropertyStr(ctx, s, "naturalHeight"));
        JS_FreeValue(ctx, p);
        if (!b) return 0;
        *px = (const uint32_t *)b; *w = ww; *h = hh;
        return ww > 0 && hh > 0;
    }
    JS_FreeValue(ctx, p);
    JSValue cx = JS_GetPropertyStr(ctx, s, "_ctx");
    if (JS_IsObject(cx)) { int r = canvas_ctx_pixels(ctx, cx, px, w, h, flip); JS_FreeValue(ctx, cx); return r; }
    JS_FreeValue(ctx, cx);
    JSValue d = JS_GetPropertyStr(ctx, s, "data");
    if (JS_IsObject(d)) {
        size_t len; uint8_t *b = bytes_of(ctx, d, &len);
        int32_t ww = 0, hh = 0;
        JS_ToInt32(ctx, &ww, JS_GetPropertyStr(ctx, s, "width")); JS_ToInt32(ctx, &hh, JS_GetPropertyStr(ctx, s, "height"));
        JS_FreeValue(ctx, d);
        if (!b) return 0;
        *px = (const uint32_t *)b; *w = ww; *h = hh;
        return 1;
    }
    JS_FreeValue(ctx, d);
    return 0;
}
FN(m_drawImage)
{
    C const uint32_t *px; int sw, sh, flip; uint8_t *own;
    if (!src_pixels(ctx, A(0), &px, &sw, &sh, &flip, &own)) return JS_UNDEFINED;
    float sx = 0, sy = 0, sW = sw, sH = sh, dx, dy, dW, dH;
    if (argc >= 9) { sx = D(1); sy = D(2); sW = D(3); sH = D(4); dx = D(5); dy = D(6); dW = D(7); dH = D(8); }
    else if (argc >= 5) { dx = D(1); dy = D(2); dW = D(3); dH = D(4); }
    else { dx = D(1); dy = D(2); dW = sw; dH = sh; }
    /* for WebGL canvases the source buffer may be smaller than its virtual size */
    JSValue cw = JS_GetPropertyStr(ctx, A(0), "width");
    int32_t vw = sw; JS_ToInt32(ctx, &vw, cw); JS_FreeValue(ctx, cw);
    float ssc = vw > 0 ? (float)sw / vw : 1;
    if (argc < 9 && ssc != 1.0f && argc < 5) { dW = vw; dH = vw ? sh / ssc : sh; }
    if (argc >= 9) { sx *= ssc; sy *= ssc; sW *= ssc; sH *= ssc; } else { sW = sw; sH = sh; }
    /* map destination rect corners; assume axis-aligned transform for speed */
    float x0, y0, x1, y1;
    xf(c, dx, dy, &x0, &y0); xf(c, dx + dW, dy + dH, &x1, &y1);
    if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }
    int ix0 = (int)floorf(x0 + 0.5f), iy0 = (int)floorf(y0 + 0.5f), ix1 = (int)floorf(x1 + 0.5f), iy1 = (int)floorf(y1 + 0.5f);
    float fw = x1 - x0, fh = y1 - y0;
    if (fw <= 0 || fh <= 0) return JS_UNDEFINED;
    for (int y = iy0 < 0 ? 0 : iy0; y < iy1 && y < c->h; y++) {
        float v = (y + 0.5f - y0) / fh;
        int ty = (int)(sy + v * sH); if (ty < 0) ty = 0; if (ty >= sh) ty = sh - 1;
        if (flip) ty = sh - 1 - ty;
        for (int x = ix0 < 0 ? 0 : ix0; x < ix1 && x < c->w; x++) {
            float u = (x + 0.5f - x0) / fw;
            int tx = (int)(sx + u * sW); if (tx < 0) tx = 0; if (tx >= sw) tx = sw - 1;
            uint32_t p = px[(size_t)ty * sw + tx];
            uint32_t argb = (p & 0xFF00FF00u) | ((p & 255) << 16) | ((p >> 16) & 255);
            plot(c, x, y, argb, 256);
        }
    }
    free(own);
    return JS_UNDEFINED;
}

FN(m_isPointInPath) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_FALSE; }
FN(m_noop2d) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
FN(m_resize2d)
{
    C int w = (int)D(0), h = (int)D(1);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    float s = 1;
    if (w > 640) s = 640.0f / w;
    if (h * s > 480) s = 480.0f / h;
    c->vw = w; c->vh = h; c->sc = s;
    c->w = (int)(w * s + 0.5f); c->h = (int)(h * s + 0.5f);
    if (c->w < 1) c->w = 1;
    if (c->h < 1) c->h = 1;
    free(c->px);
    c->px = calloc((size_t)c->w * c->h, 4);
    /* resetting the canvas resets state */
    float *m = c->st.m; m[0] = 1; m[1] = 0; m[2] = 0; m[3] = 1; m[4] = 0; m[5] = 0;
    c->sp = 0;
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry c2d_funcs[] = {
    JS_CFUNC_DEF("beginPath", 0, m_beginPath), JS_CFUNC_DEF("moveTo", 2, m_moveTo), JS_CFUNC_DEF("lineTo", 2, m_lineTo),
    JS_CFUNC_DEF("closePath", 0, m_closePath), JS_CFUNC_DEF("rect", 4, m_rect), JS_CFUNC_DEF("arc", 5, m_arc),
    JS_CFUNC_DEF("ellipse", 7, m_ellipse), JS_CFUNC_DEF("arcTo", 5, m_arcTo), JS_CFUNC_DEF("quadraticCurveTo", 4, m_quadTo),
    JS_CFUNC_DEF("bezierCurveTo", 6, m_bezierTo), JS_CFUNC_DEF("fill", 0, m_fill), JS_CFUNC_DEF("stroke", 0, m_stroke),
    JS_CFUNC_DEF("fillRect", 4, m_fillRect), JS_CFUNC_DEF("clearRect", 4, m_clearRect), JS_CFUNC_DEF("strokeRect", 4, m_strokeRect),
    JS_CFUNC_DEF("fillText", 3, m_fillText), JS_CFUNC_DEF("strokeText", 3, m_strokeText), JS_CFUNC_DEF("measureText", 1, m_measureText),
    JS_CFUNC_DEF("translate", 2, m_translate), JS_CFUNC_DEF("scale", 2, m_scale), JS_CFUNC_DEF("rotate", 1, m_rotate),
    JS_CFUNC_DEF("transform", 6, m_transform), JS_CFUNC_DEF("setTransform", 6, m_setTransform), JS_CFUNC_DEF("resetTransform", 0, m_setTransform),
    JS_CFUNC_DEF("save", 0, m_save), JS_CFUNC_DEF("restore", 0, m_restore),
    JS_CFUNC_DEF("createImageData", 2, m_createImageData), JS_CFUNC_DEF("getImageData", 4, m_getImageData),
    JS_CFUNC_DEF("putImageData", 3, m_putImageData), JS_CFUNC_DEF("drawImage", 3, m_drawImage),
    JS_CFUNC_DEF("isPointInPath", 2, m_isPointInPath), JS_CFUNC_DEF("isPointInStroke", 2, m_isPointInPath),
    JS_CFUNC_DEF("setLineDash", 1, m_noop2d), JS_CFUNC_DEF("clip", 0, m_noop2d), JS_CFUNC_DEF("__resize", 2, m_resize2d),
    JS_CGETSET_DEF("fillStyle", g_fillStyle, s_fillStyle), JS_CGETSET_DEF("strokeStyle", g_strokeStyle, s_strokeStyle),
    JS_CGETSET_DEF("lineWidth", g_lineWidth, s_lineWidth), JS_CGETSET_DEF("globalAlpha", g_alpha, s_alpha),
    JS_CGETSET_DEF("globalCompositeOperation", g_comp, s_comp), JS_CGETSET_DEF("font", g_font, s_font),
    JS_CGETSET_DEF("textAlign", g_align, s_align), JS_CGETSET_DEF("textBaseline", g_base, s_base),
};

static void c2d_finalizer(JSRuntime *rt, JSValue val)
{
    (void)rt;
    C2D *c = JS_GetOpaque(val, c2d_cid);
    if (!c) return;
    free(c->px); free(c->pts); free(c->subs); free(c);
}
static JSClassDef c2d_class = { "CanvasRenderingContext2D", .finalizer = c2d_finalizer };

static const char *c2d_js =
    "(function(P){"
    "function Grad(){this.stops=[];this._color='#000';}"
    "Grad.prototype.addColorStop=function(o,c){this.stops.push([o,c]);if(this.stops.length===1||o<=0.5)this._color=c;};"
    "P.createLinearGradient=function(){return new Grad();};"
    "P.createRadialGradient=function(){return new Grad();};"
    "P.createConicGradient=function(){return new Grad();};"
    "P.createPattern=function(){var g=new Grad();g._color='#888';return g;};"
    "P.getLineDash=function(){return [];};"
    "P.getTransform=function(){return {a:1,b:0,c:0,d:1,e:0,f:0};};"
    "P.lineCap='butt';P.lineJoin='miter';P.miterLimit=10;P.lineDashOffset=0;P.shadowBlur=0;P.shadowColor='rgba(0,0,0,0)';"
    "P.shadowOffsetX=0;P.shadowOffsetY=0;P.imageSmoothingEnabled=true;P.filter='none';P.direction='ltr';"
    "P.roundRect=function(x,y,w,h){this.rect(x,y,w,h);};"
    "})";

void canvas2d_init(JSContext *ctx)
{
    JS_NewClassID(&c2d_cid);
    JS_NewClass(JS_GetRuntime(ctx), c2d_cid, &c2d_class);
    c2d_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, c2d_proto, c2d_funcs, sizeof c2d_funcs / sizeof c2d_funcs[0]);
    JSValue f = JS_Eval(ctx, c2d_js, strlen(c2d_js), "<canvas2d>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, f)) { JSValue r = JS_Call(ctx, f, JS_UNDEFINED, 1, (JSValueConst *)&c2d_proto); JS_FreeValue(ctx, r); }
    JS_FreeValue(ctx, f);
    JS_SetClassProto(ctx, c2d_cid, JS_DupValue(ctx, c2d_proto));
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ctor, "prototype", JS_DupValue(ctx, c2d_proto));
    JS_SetPropertyStr(ctx, global, "CanvasRenderingContext2D", ctor);
    JS_FreeValue(ctx, global);
}
void canvas2d_free_protos(JSContext *ctx) { JS_FreeValue(ctx, c2d_proto); }

JSValue canvas2d_create(JSContext *ctx, JSValueConst canvas)
{
    C2D *c = calloc(1, sizeof(C2D));
    c->st.fill = 0xFF000000u; c->st.stroke = 0xFF000000u; c->st.lw = 1; c->st.alpha = 1;
    c->st.m[0] = 1; c->st.m[3] = 1; c->st.font = FNT_SANS; c->st.fsize = 10;
    JSValue o = JS_NewObjectProtoClass(ctx, c2d_proto, c2d_cid);
    JS_SetOpaque(o, c);
    int32_t w = 300, h = 150;
    JS_ToInt32(ctx, &w, JS_GetPropertyStr(ctx, canvas, "width"));
    JS_ToInt32(ctx, &h, JS_GetPropertyStr(ctx, canvas, "height"));
    JSValue args[2] = { JS_NewInt32(ctx, w), JS_NewInt32(ctx, h) };
    m_resize2d(ctx, o, 2, args);
    JS_SetPropertyStr(ctx, o, "canvas", JS_DupValue(ctx, canvas));
    return o;
}

int canvas_ctx_pixels(JSContext *ctx, JSValueConst ctxobj, const uint32_t **px, int *w, int *h, int *flip)
{
    C2D *c = JS_GetOpaque(ctxobj, c2d_cid);
    if (c) { *px = c->px; *w = c->w; *h = c->h; *flip = 0; return 1; }
    int alpha;
    if (webgl_pixels(ctx, ctxobj, px, w, h, &alpha)) { *flip = 1; return 1; }
    return 0;
}
int canvas2d_is(JSValueConst ctxobj) { return JS_GetOpaque(ctxobj, c2d_cid) != NULL; }
