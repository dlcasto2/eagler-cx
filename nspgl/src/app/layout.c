/* Simple flow layout of the JS DOM into a display list */
#include "page.h"
#include "gfx.h"
#include "css.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

void page_compute_styles(Page *pg);

typedef struct Pend { JSValue el; int i0, i1; int x, y, w, h; int block; } Pend;

typedef struct L {
    Page *pg; JSContext *ctx;
    int x0, width;
    int y;
    int lx, line_start, line_asc, line_desc, line_has;
    int align;
    int link;
    uint32_t color;
    int font, pre, nowrap, hidden;
    int fixed;
    int pending_space;
    Pend *pend; int npend, cappend;
    JSValue *abs; int nabs, capabs;
    int in_abs;
} L;

static DItem *add_item(L *l, int type)
{
    Page *pg = l->pg;
    if (pg->nitems == pg->capitems) { pg->capitems = pg->capitems ? pg->capitems * 2 : 128; pg->items = realloc(pg->items, pg->capitems * sizeof(DItem)); }
    DItem *it = &pg->items[pg->nitems++];
    memset(it, 0, sizeof *it);
    it->type = type; it->link = l->link; it->obj = JS_UNDEFINED; it->fixed = l->fixed;
    return it;
}

/* ---------- JS helpers ---------- */
static char *jstr(JSContext *ctx, JSValueConst o, const char *prop, char *buf, int sz)
{
    buf[0] = 0;
    JSValue v = JS_GetPropertyStr(ctx, o, prop);
    if (JS_IsString(v)) { const char *s = JS_ToCString(ctx, v); if (s) snprintf(buf, sz, "%s", s); JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, v);
    return buf[0] ? buf : NULL;
}
static int jint(JSContext *ctx, JSValueConst o, const char *prop, int dflt)
{
    JSValue v = JS_GetPropertyStr(ctx, o, prop);
    int32_t r = dflt;
    if (JS_IsNumber(v)) JS_ToInt32(ctx, &r, v);
    JS_FreeValue(ctx, v);
    return r;
}
static char *attr(JSContext *ctx, JSValueConst el, const char *name, char *buf, int sz)
{
    JSValue a = JS_GetPropertyStr(ctx, el, "_attrs");
    char *r = jstr(ctx, a, name, buf, sz);
    JS_FreeValue(ctx, a);
    return r;
}
static int has_attr(JSContext *ctx, JSValueConst el, const char *name)
{
    JSValue a = JS_GetPropertyStr(ctx, el, "_attrs");
    JSAtom at = JS_NewAtom(ctx, name);
    int r = JS_HasProperty(ctx, a, at) > 0;
    JS_FreeAtom(ctx, at); JS_FreeValue(ctx, a);
    return r;
}

/* ---------- lines ---------- */
static void flush_line(L *l)
{
    Page *pg = l->pg;
    if (!l->line_has) { l->lx = l->x0; l->pending_space = 0; return; }
    int top = l->y;
    int shift = 0, used = l->lx - l->x0;
    if (l->align == 1) shift = (l->width - used) / 2;
    else if (l->align == 2) shift = l->width - used;
    if (shift < 0) shift = 0;
    for (int i = l->line_start; i < pg->nitems; i++) {
        DItem *it = &pg->items[i];
        if (it->type == DI_TEXT) it->y = top + l->line_asc - gfx_font_ascent(it->font);
        else if (it->type == DI_BULLET) it->y = top + l->line_asc - gfx_font_ascent(it->font);
        else it->y = top + l->line_asc - it->h;
        it->x += shift;
    }
    l->y = top + l->line_asc + l->line_desc;
    l->lx = l->x0;
    l->line_start = pg->nitems;
    l->line_asc = l->line_desc = 0;
    l->line_has = 0;
    l->pending_space = 0;
}
static void line_box(L *l, int asc, int desc)
{
    if (asc > l->line_asc) l->line_asc = asc;
    if (desc > l->line_desc) l->line_desc = desc;
    l->line_has = 1;
}

static void emit_word(L *l, const char *w, int n, int space_before)
{
    int f = l->font;
    int ww = gfx_text_width(f, w, n);
    int sw = space_before ? gfx_char_width(f, ' ') : 0;
    if (l->line_has && !l->nowrap && l->lx + sw + ww > l->x0 + l->width) { flush_line(l); sw = 0; }
    if (!l->line_has) sw = 0;
    Page *pg = l->pg;
    DItem *last = pg->nitems > l->line_start ? &pg->items[pg->nitems - 1] : NULL;
    int asc = gfx_font_ascent(f), desc = gfx_font_height(f) - asc;
    if (last && last->type == DI_TEXT && last->font == f && last->color == l->color && last->link == l->link &&
        last->x + last->w == l->lx && !l->hidden) {
        last->text = realloc(last->text, last->len + n + 2);
        if (sw) last->text[last->len++] = ' ';
        memcpy(last->text + last->len, w, n);
        last->len += n; last->text[last->len] = 0;
        last->w += sw + ww;
    } else if (!l->hidden) {
        DItem *it = add_item(l, DI_TEXT);
        it->font = f; it->color = l->color;
        it->x = l->lx + sw; it->w = ww; it->h = gfx_font_height(f);
        it->text = malloc(n + 1); memcpy(it->text, w, n); it->text[n] = 0; it->len = n;
    }
    l->lx += sw + ww;
    line_box(l, asc, desc);
}

static void emit_text(L *l, const char *s, int n)
{
    if (l->pre) {
        int i = 0;
        while (i < n) {
            int j = i;
            while (j < n && s[j] != '\n') j++;
            if (j > i) {
                /* expand tabs */
                char buf[1024]; int o = 0;
                for (int k = i; k < j && o < 1000; k++) {
                    if (s[k] == '\t') { do buf[o++] = ' '; while (o % 4 && o < 1000); }
                    else if (s[k] != '\r') buf[o++] = s[k];
                }
                emit_word(l, buf, o, 0);
            }
            if (j < n) {   /* newline */
                if (!l->line_has) line_box(l, gfx_font_ascent(l->font), gfx_font_height(l->font) - gfx_font_ascent(l->font));
                flush_line(l);
            }
            i = j + 1;
        }
        return;
    }
    int i = 0;
    while (i < n) {
        if (isspace((unsigned char)s[i])) { l->pending_space = 1; i++; continue; }
        int j = i;
        while (j < n && !isspace((unsigned char)s[j])) j++;
        emit_word(l, s + i, j - i, l->pending_space);
        l->pending_space = 0;
        i = j;
    }
}

static void add_pend(L *l, JSValue el, int i0, int x, int y, int w, int h, int block)
{
    if (l->npend == l->cappend) { l->cappend = l->cappend ? l->cappend * 2 : 64; l->pend = realloc(l->pend, l->cappend * sizeof(Pend)); }
    Pend *p = &l->pend[l->npend++];
    p->el = JS_DupValue(l->ctx, el); p->i0 = i0; p->i1 = l->pg->nitems; p->x = x; p->y = y; p->w = w; p->h = h; p->block = block;
}

static int add_link(L *l, JSValue el)
{
    Page *pg = l->pg;
    if (pg->nlinks == pg->caplinks) { pg->caplinks = pg->caplinks ? pg->caplinks * 2 : 16; pg->links = realloc(pg->links, pg->caplinks * sizeof(Link)); }
    Link *k = &pg->links[pg->nlinks];
    k->el = JS_DupValue(l->ctx, el); k->x = k->y = k->w = k->h = 0; k->fixed = l->fixed;
    return pg->nlinks++;
}

/* ---------- elements ---------- */
static const char *block_tags[] = {"html","body","div","p","h1","h2","h3","h4","h5","h6","ul","ol","li","dl","dt","dd","pre",
    "blockquote","hr","table","tr","form","header","footer","section","article","nav","main","aside","figure","figcaption",
    "center","address","details","summary","fieldset","legend","thead","tbody","tfoot","caption","menu","hgroup","dialog",NULL};
static const char *none_tags[] = {"head","script","style","title","meta","link","noscript","template","base","param","source","track",NULL};
static int in_list(const char *t, const char **l) { for (int i = 0; l[i]; i++) if (!strcmp(t, l[i])) return 1; return 0; }

static void layout_node(L *l, JSValue n, int depth);

static void layout_children(L *l, JSValue el, int depth)
{
    JSContext *ctx = l->ctx;
    JSValue kids = JS_GetPropertyStr(ctx, el, "childNodes");
    int nk = jint(ctx, kids, "length", 0);
    for (int i = 0; i < nk; i++) {
        JSValue k = JS_GetPropertyUint32(ctx, kids, i);
        layout_node(l, k, depth + 1);
        JS_FreeValue(ctx, k);
    }
    JS_FreeValue(ctx, kids);
}

static int font_for(const char *tag, int cur, JSContext *ctx, JSValue cs)
{
    int f = cur;
    if (!strcmp(tag, "h1")) f = FNT_H1;
    else if (!strcmp(tag, "h2")) f = FNT_H2;
    else if (tag[0] == 'h' && tag[1] >= '3' && tag[1] <= '6' && !tag[2]) f = FNT_BOLD;
    else if (!strcmp(tag, "b") || !strcmp(tag, "strong") || !strcmp(tag, "th") || !strcmp(tag, "dt") || !strcmp(tag, "summary") || !strcmp(tag, "legend")) f = (cur == FNT_H1 || cur == FNT_H2) ? cur : FNT_BOLD;
    else if (!strcmp(tag, "i") || !strcmp(tag, "em") || !strcmp(tag, "cite") || !strcmp(tag, "var")) f = (cur == FNT_SANS) ? FNT_ITAL : cur;
    else if (!strcmp(tag, "code") || !strcmp(tag, "pre") || !strcmp(tag, "kbd") || !strcmp(tag, "tt") || !strcmp(tag, "samp")) f = FNT_MONO;
    else if (!strcmp(tag, "small") || !strcmp(tag, "sub") || !strcmp(tag, "sup")) f = FNT_SMALL;
    char b[64];
    if (jstr(ctx, cs, "fontSize", b, sizeof b)) {
        float px = css_len(b, 13, 13);
        if (strstr(b, "em") && !strstr(b, "rem")) px = px;   /* relative to 13px base */
        if (px >= 20) f = FNT_H1; else if (px >= 15) f = FNT_H2; else if (px <= 10) f = FNT_SMALL;
        else if (f == FNT_H1 || f == FNT_H2 || f == FNT_SMALL) f = FNT_SANS;
    }
    if (jstr(ctx, cs, "fontWeight", b, sizeof b) && (!strcmp(b, "bold") || atoi(b) >= 600) && (f == FNT_SANS || f == FNT_ITAL)) f = FNT_BOLD;
    if (jstr(ctx, cs, "fontFamily", b, sizeof b) && (strstr(b, "mono") || strstr(b, "Courier") || strstr(b, "Consol")) && f == FNT_SANS) f = FNT_MONO;
    return f;
}

static void layout_replaced(L *l, JSValue el, const char *tag, JSValue cs)
{
    JSContext *ctx = l->ctx;
    char b[64];
    int iw, ih;
    int is_canvas = !strcmp(tag, "canvas");
    if (is_canvas) { iw = jint(ctx, el, "width", 300); ih = jint(ctx, el, "height", 150); }
    else { iw = jint(ctx, el, "width", 0); ih = jint(ctx, el, "height", 0); if (!iw) iw = 16; if (!ih) ih = 16; }
    float w = iw, h = ih;
    int cw = jstr(ctx, cs, "width", b, sizeof b) && strcmp(b, "auto") ? 1 : 0;
    float W = cw ? css_len(b, l->width, iw) : iw;
    int ch = jstr(ctx, cs, "height", b, sizeof b) && strcmp(b, "auto") ? 1 : 0;
    float H = ch ? css_len(b, l->in_abs ? SCREEN_H_CSS : SCREEN_H_CSS, ih) : ih;
    if (cw && !ch && iw) H = W * ih / iw;
    if (ch && !cw && ih) W = H * iw / ih;
    w = W; h = H;
    /* fit to available width / screen height */
    if (w > l->width && w > 0) { h = h * l->width / w; w = l->width; }
    if (h > SCREEN_H_CSS && h > 0) { w = w * SCREEN_H_CSS / h; h = SCREEN_H_CSS; }
    int wi = (int)(w + 0.5f), hi = (int)(h + 0.5f);
    if (wi < 1) wi = 1;
    if (hi < 1) hi = 1;
    if (l->line_has && l->lx + wi > l->x0 + l->width) flush_line(l);
    int i0 = l->pg->nitems;
    DItem *it = add_item(l, is_canvas ? DI_CANVAS : DI_IMAGE);
    it->x = l->lx; it->w = wi; it->h = hi; it->obj = JS_DupValue(ctx, el);
    if (l->hidden) it->type = DI_RECT, it->color = 0;
    l->lx += wi;
    line_box(l, hi, 0);
    add_pend(l, el, i0, 0, 0, 0, 0, 0);
}

static void layout_control(L *l, JSValue el, const char *tag)
{
    JSContext *ctx = l->ctx;
    char b[128], t[32];
    const char *type = attr(ctx, el, "type", t, sizeof t);
    const char *txt = "";
    JSValue v = JS_GetPropertyStr(ctx, el, "value");
    const char *vs = JS_ToCString(ctx, v);
    snprintf(b, sizeof b, "%s", vs ? vs : "");
    JS_FreeCString(ctx, vs); JS_FreeValue(ctx, v);
    int check = type && (!strcmp(type, "checkbox") || !strcmp(type, "radio"));
    if (type && !strcmp(type, "hidden")) return;
    if (check) {
        JSValue c = JS_GetPropertyStr(ctx, el, "checked");
        snprintf(b, sizeof b, "%s", JS_ToBool(ctx, c) ? "[x]" : "[ ]");
        JS_FreeValue(ctx, c);
    } else if (!strcmp(tag, "button")) {
        JSValue c = JS_GetPropertyStr(ctx, el, "textContent");
        const char *s = JS_ToCString(ctx, c);
        snprintf(b, sizeof b, "%s", s ? s : "");
        JS_FreeCString(ctx, s); JS_FreeValue(ctx, c);
    } else if (!strcmp(tag, "select")) {
        JSValue o = JS_GetPropertyStr(ctx, el, "firstElementChild");
        if (JS_IsObject(o)) { JSValue c = JS_GetPropertyStr(ctx, o, "textContent"); const char *s = JS_ToCString(ctx, c); snprintf(b, sizeof b, "%s v", s ? s : ""); JS_FreeCString(ctx, s); JS_FreeValue(ctx, c); }
        JS_FreeValue(ctx, o);
    }
    txt = b;
    /* collapse whitespace */
    char clean[128]; int o = 0, sp = 0;
    for (const char *p = txt; *p && o < 120; p++) {
        if (isspace((unsigned char)*p)) { sp = 1; continue; }
        if (sp && o) clean[o++] = ' ';
        sp = 0; clean[o++] = *p;
    }
    clean[o] = 0;
    int f = FNT_SANS;
    int tw = gfx_text_width(f, clean, o);
    int isbtn = !strcmp(tag, "button") || (type && (!strcmp(type, "button") || !strcmp(type, "submit") || !strcmp(type, "reset")));
    int w = check ? tw + 2 : (isbtn ? tw + 10 : (tw + 10 > 80 ? tw + 10 : 80));
    if (w > l->width) w = l->width;
    int h = gfx_font_height(f) + 4;
    if (l->line_has && l->lx + w > l->x0 + l->width) flush_line(l);
    int old = l->link;
    l->link = add_link(l, el);
    int i0 = l->pg->nitems;
    if (!check) {
        DItem *bg = add_item(l, DI_RECT); bg->x = l->lx; bg->w = w; bg->h = h; bg->color = isbtn ? 0xFFDDDDDD : 0xFFFFFFFF;
        DItem *br = add_item(l, DI_BORDER); br->x = l->lx; br->w = w; br->h = h; br->color = 0xFF777777;
    }
    DItem *tx = add_item(l, DI_TEXT);
    tx->font = f; tx->color = 0xFF000000; tx->x = l->lx + (check ? 1 : (isbtn ? (w - tw) / 2 : 4)); tx->w = tw; tx->h = h;
    tx->text = strdup(clean); tx->len = o;
    /* text item gets aligned like a box: mark h as box height and shift by 2 */
    tx->type = DI_TEXT;
    l->lx += w + 2;
    line_box(l, h, 0);
    /* adjust text y relative: flush uses ascent for text; emulate with BULLET-style later */
    l->link = old;
    add_pend(l, el, i0, 0, 0, 0, 0, 0);
}

static void layout_node(L *l, JSValue n, int depth)
{
    JSContext *ctx = l->ctx;
    if (depth > 200) return;
    int type = jint(ctx, n, "nodeType", 0);
    if (type == 3) {
        JSValue d = JS_GetPropertyStr(ctx, n, "data");
        size_t len; const char *s = JS_ToCStringLen(ctx, &len, d);
        if (s) emit_text(l, s, (int)len);
        JS_FreeCString(ctx, s); JS_FreeValue(ctx, d);
        return;
    }
    if (type != 1) return;
    char tag[32], b[96];
    jstr(ctx, n, "localName", tag, sizeof tag);
    JSValue cs = JS_GetPropertyStr(ctx, n, "__cs");
    char disp[32] = "";
    jstr(ctx, cs, "display", disp, sizeof disp);
    if (!strcmp(disp, "none") || (!disp[0] && in_list(tag, none_tags))) { JS_FreeValue(ctx, cs); return; }
    char pos[16] = "";
    jstr(ctx, cs, "position", pos, sizeof pos);
    if (!l->in_abs && (!strcmp(pos, "absolute") || !strcmp(pos, "fixed"))) {
        if (l->nabs == l->capabs) { l->capabs = l->capabs ? l->capabs * 2 : 8; l->abs = realloc(l->abs, l->capabs * sizeof(JSValue)); }
        l->abs[l->nabs++] = JS_DupValue(ctx, n);
        JS_FreeValue(ctx, cs);
        return;
    }
    int block = disp[0] ? (strcmp(disp, "inline") && strcmp(disp, "inline-block") && strcmp(disp, "contents")) : in_list(tag, block_tags);
    if (!strcmp(tag, "td") || !strcmp(tag, "th")) block = 0;
    /* save inherited state */
    L save = *l;
    l->font = font_for(tag, l->font, ctx, cs);
    uint32_t col;
    if (jstr(ctx, cs, "color", b, sizeof b) && css_color(b, &col)) l->color = col;
    char vis[16];
    if (jstr(ctx, cs, "visibility", vis, sizeof vis) && !strcmp(vis, "hidden")) l->hidden = 1;
    char ws[16];
    if (!strcmp(tag, "pre") || !strcmp(tag, "textarea") || (jstr(ctx, cs, "whiteSpace", ws, sizeof ws) && !strncmp(ws, "pre", 3))) l->pre = 1;
    if (jstr(ctx, cs, "whiteSpace", ws, sizeof ws) && !strcmp(ws, "nowrap")) l->nowrap = 1;
    int is_link = !strcmp(tag, "a") && has_attr(ctx, n, "href");
    if (is_link || has_attr(ctx, n, "onclick")) {
        l->link = add_link(l, n);
        if (is_link && !(jstr(ctx, cs, "color", b, sizeof b))) l->color = 0xFF0645AD;
    }
    if (!strcmp(tag, "canvas") || !strcmp(tag, "img") || !strcmp(tag, "video")) {
        if (block) flush_line(l);
        layout_replaced(l, n, tag, cs);
        if (block) flush_line(l);
        goto restore;
    }
    if (!strcmp(tag, "input") || !strcmp(tag, "button") || !strcmp(tag, "select") || !strcmp(tag, "textarea")) {
        layout_control(l, n, tag);
        goto restore;
    }
    if (!strcmp(tag, "br")) {
        if (!l->line_has) line_box(l, gfx_font_ascent(l->font), gfx_font_height(l->font) - gfx_font_ascent(l->font));
        flush_line(l);
        goto restore;
    }
    int i0 = l->pg->nitems;
    int bx = l->x0, by = 0, bw = l->width;
    int bg_item = -1;
    if (block) {
        flush_line(l);
        int mt = 0, mb = 0, indent = 0;
        if (!strcmp(tag, "p")) mt = mb = 6;
        else if (!strcmp(tag, "h1")) mt = mb = 8;
        else if (!strcmp(tag, "h2")) mt = mb = 6;
        else if (tag[0] == 'h' && isdigit((unsigned char)tag[1]) && !tag[2]) mt = mb = 4;
        else if (!strcmp(tag, "ul") || !strcmp(tag, "ol")) { mt = mb = depth > 6 ? 0 : 4; indent = 14; }
        else if (!strcmp(tag, "pre") || !strcmp(tag, "blockquote") || !strcmp(tag, "dl") || !strcmp(tag, "table")) { mt = mb = 4; }
        if (!strcmp(tag, "blockquote") || !strcmp(tag, "dd")) indent = 14;
        if (jstr(ctx, cs, "marginTop", b, sizeof b)) mt = (int)css_len(b, l->width, mt);
        if (jstr(ctx, cs, "marginBottom", b, sizeof b)) mb = (int)css_len(b, l->width, mb);
        if (jstr(ctx, cs, "margin", b, sizeof b) && strcmp(tag, "body")) {
            float v = css_len(b, l->width, -1);
            if (v >= 0) mt = mb = (int)v;
        }
        if (mt > 24) mt = 24;
        if (mb > 24) mb = 24;
        l->y += mt;
        by = l->y;
        char ta[16];
        if (jstr(ctx, cs, "textAlign", ta, sizeof ta)) l->align = !strcmp(ta, "center") ? 1 : !strcmp(ta, "right") ? 2 : 0;
        char jc[24];
        if (!strcmp(disp, "flex") && jstr(ctx, cs, "justifyContent", jc, sizeof jc) && !strcmp(jc, "center")) l->align = 1;
        int pad = 0;
        if (jstr(ctx, cs, "padding", b, sizeof b)) { pad = (int)css_len(b, l->width, 0); if (pad > 16) pad = 16; if (pad < 0) pad = 0; }
        uint32_t bgc;
        if ((jstr(ctx, cs, "backgroundColor", b, sizeof b) || jstr(ctx, cs, "background", b, sizeof b)) && css_color(b, &bgc) && (bgc >> 24)) {
            if (!strcmp(tag, "body") || !strcmp(tag, "html")) l->pg->bg = bgc | 0xFF000000u;
            else { DItem *it = add_item(l, DI_RECT); it->color = bgc; it->x = bx; it->w = bw; bg_item = l->pg->nitems - 1; }
        }
        if (!strcmp(tag, "hr")) {
            DItem *it = add_item(l, DI_HR); it->x = l->x0; it->y = l->y + 3; it->w = l->width; it->h = 1; it->color = 0xFF999999;
            l->y += 7;
        }
        l->x0 += indent + pad; l->width -= indent + 2 * pad;
        l->y += pad;
        if (l->width < 20) l->width = 20;
        l->lx = l->x0; l->line_start = l->pg->nitems;
        if (!strcmp(tag, "li")) {
            /* bullet / number */
            JSValue par = JS_GetPropertyStr(ctx, n, "parentNode");
            char pt[16] = "";
            if (JS_IsObject(par)) jstr(ctx, par, "localName", pt, sizeof pt);
            char mark[16];
            if (!strcmp(pt, "ol")) {
                JSValue kids = JS_GetPropertyStr(ctx, par, "children");
                int nk = jint(ctx, kids, "length", 0), idx = 1;
                for (int i = 0; i < nk; i++) { JSValue k = JS_GetPropertyUint32(ctx, kids, i); int same = JS_VALUE_GET_PTR(k) == JS_VALUE_GET_PTR(n); JS_FreeValue(ctx, k); if (same) { idx = i + 1; break; } }
                JS_FreeValue(ctx, kids);
                snprintf(mark, sizeof mark, "%d.", idx);
            } else strcpy(mark, "\xC2\xB7");
            JS_FreeValue(ctx, par);
            int mw = gfx_text_width(l->font, mark, strlen(mark));
            DItem *it = add_item(l, DI_BULLET);
            it->font = l->font; it->color = l->color; it->x = l->x0 - mw - 4; it->w = mw; it->h = gfx_font_height(l->font);
            it->text = strdup(mark); it->len = strlen(mark);
            l->line_start = l->pg->nitems - 1;
        }
        if (!strcmp(tag, "tr")) {
            /* table row: lay cells side by side */
            JSValue kids = JS_GetPropertyStr(ctx, n, "children");
            int nk = jint(ctx, kids, "length", 0);
            if (nk > 0) {
                int colw = l->width / nk, top = l->y, maxy = l->y;
                int ox0 = l->x0, ow = l->width;
                for (int i = 0; i < nk; i++) {
                    JSValue k = JS_GetPropertyUint32(ctx, kids, i);
                    l->x0 = ox0 + i * colw + 1; l->width = colw - 4; l->y = top; l->lx = l->x0; l->line_start = l->pg->nitems;
                    L s2 = *l;
                    char ct[8]; jstr(ctx, k, "localName", ct, sizeof ct);
                    if (!strcmp(ct, "th")) l->font = FNT_BOLD;
                    layout_children(l, k, depth + 1);
                    flush_line(l);
                    l->font = s2.font;
                    int cy = l->y;
                    add_pend(l, k, s2.line_start, l->x0, top, l->width, cy - top, 1);
                    if (cy > maxy) maxy = cy;
                    JS_FreeValue(ctx, k);
                }
                l->x0 = ox0; l->width = ow; l->y = maxy + 2;
            }
            JS_FreeValue(ctx, kids);
        } else layout_children(l, n, depth);
        flush_line(l);
        l->y += pad;
        if (!strcmp(tag, "canvas")) {}
        int bh = l->y - by;
        if (bg_item >= 0) { DItem *it = &l->pg->items[bg_item]; it->y = by - pad; it->h = bh + pad; }
        l->y += mb;
        add_pend(l, n, i0, bx, by, bw, bh, 1);
    } else {
        layout_children(l, n, depth);
        add_pend(l, n, i0, 0, 0, 0, 0, 0);
    }
restore:;
    /* restore inherited state (but keep flow position) */
    int y = l->y, lx = l->lx, ls = l->line_start, la = l->line_asc, ld = l->line_desc, lh = l->line_has, ps = l->pending_space;
    Pend *pd = l->pend; int np = l->npend, cp = l->cappend;
    JSValue *ab = l->abs; int na = l->nabs, ca = l->capabs;
    *l = save;
    l->y = y; l->lx = lx; l->line_start = ls; l->line_asc = la; l->line_desc = ld; l->line_has = lh; l->pending_space = ps;
    l->pend = pd; l->npend = np; l->cappend = cp; l->abs = ab; l->nabs = na; l->capabs = ca;
    if (block) { l->lx = l->x0; }
    JS_FreeValue(ctx, cs);
}

static void layout_abs(L *l, JSValue el)
{
    JSContext *ctx = l->ctx;
    JSValue cs = JS_GetPropertyStr(ctx, el, "__cs");
    char b[64], pos[16] = "";
    jstr(ctx, cs, "position", pos, sizeof pos);
    int fixed = !strcmp(pos, "fixed");
    float left = 0, top = 0;
    int hasl = 0, hast = 0, hasr = 0, hasb = 0;
    float right = 0, bottom = 0, w = SCREEN_W_CSS;
    if (jstr(ctx, cs, "left", b, sizeof b) && strcmp(b, "auto")) { left = css_len(b, SCREEN_W_CSS, 0); hasl = 1; }
    if (jstr(ctx, cs, "top", b, sizeof b) && strcmp(b, "auto")) { top = css_len(b, SCREEN_H_CSS, 0); hast = 1; }
    if (jstr(ctx, cs, "right", b, sizeof b) && strcmp(b, "auto")) { right = css_len(b, SCREEN_W_CSS, 0); hasr = 1; }
    if (jstr(ctx, cs, "bottom", b, sizeof b) && strcmp(b, "auto")) { bottom = css_len(b, SCREEN_H_CSS, 0); hasb = 1; }
    if (jstr(ctx, cs, "width", b, sizeof b) && strcmp(b, "auto")) w = css_len(b, SCREEN_W_CSS, SCREEN_W_CSS);
    else if (hasl && hasr) w = SCREEN_W_CSS - left - right;
    else if (!hasl && !hasr) w = SCREEN_W_CSS;
    else w = SCREEN_W_CSS - (hasl ? left : right);
    if (w < 8) w = 8;
    if (w > SCREEN_W_CSS) w = SCREEN_W_CSS;
    JS_FreeValue(ctx, cs);
    L s = *l;
    l->in_abs = 1; l->fixed = fixed;
    l->x0 = hasl ? (int)left : hasr ? (int)(SCREEN_W_CSS - right - w) : 0;
    l->width = (int)w; l->y = (int)top; l->lx = l->x0; l->line_start = l->pg->nitems; l->line_has = 0; l->align = 0;
    l->font = FNT_SANS; l->color = 0xFF000000; l->link = -1;
    int i0 = l->pg->nitems;
    layout_node(l, el, 1);
    flush_line(l);
    int i1 = l->pg->nitems;
    /* shrink-to-fit for right anchoring; bottom anchoring */
    int minx = 1 << 30, maxx = -(1 << 30), maxy = (int)top;
    for (int i = i0; i < i1; i++) { DItem *it = &l->pg->items[i]; if (it->x < minx) minx = it->x; if (it->x + it->w > maxx) maxx = it->x + it->w; if (it->y + it->h > maxy) maxy = it->y + it->h; }
    int dx = 0, dy = 0;
    if (!hasl && hasr && maxx > minx) dx = (int)(SCREEN_W_CSS - right) - maxx;
    if (!hast && hasb) dy = (int)(SCREEN_H_CSS - bottom) - maxy;
    if (dx || dy) for (int i = i0; i < i1; i++) { l->pg->items[i].x += dx; l->pg->items[i].y += dy; }
    for (int i = 0; i < l->npend; i++) if (l->pend[i].i0 >= i0 && l->pend[i].block) { l->pend[i].x += dx; l->pend[i].y += dy; }
    Pend *pd = l->pend; int np = l->npend, cp = l->cappend;
    JSValue *ab = l->abs; int na = l->nabs, ca = l->capabs;
    *l = s;
    l->pend = pd; l->npend = np; l->cappend = cp; l->abs = ab; l->nabs = na; l->capabs = ca;
}

void page_layout(Page *pg)
{
    JSContext *ctx = pg->ctx;
    page_compute_styles(pg);
    for (int i = 0; i < pg->nitems; i++) { free(pg->items[i].text); JS_FreeValue(ctx, pg->items[i].obj); }
    pg->nitems = 0;
    for (int i = 0; i < pg->nlinks; i++) JS_FreeValue(ctx, pg->links[i].el);
    pg->nlinks = 0;
    pg->bg = 0xFFFFFFFF;
    L l; memset(&l, 0, sizeof l);
    l.pg = pg; l.ctx = ctx;
    l.font = FNT_SANS; l.color = 0xFF000000; l.link = -1;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, g, "document");
    JSValue body = JS_GetPropertyStr(ctx, doc, "body");
    JSValue html = JS_GetPropertyStr(ctx, doc, "documentElement");
    int margin = 4;
    char b[64];
    if (JS_IsObject(body)) {
        JSValue cs = JS_GetPropertyStr(ctx, body, "__cs");
        if (jstr(ctx, cs, "margin", b, sizeof b)) margin = (int)css_len(b, SCREEN_W_CSS, 4);
        if (margin > 8) margin = 8;
        if (margin < 0) margin = 0;
        uint32_t c;
        JSValue hcs = JS_GetPropertyStr(ctx, html, "__cs");
        if ((jstr(ctx, hcs, "backgroundColor", b, sizeof b) || jstr(ctx, hcs, "background", b, sizeof b)) && css_color(b, &c) && (c >> 24)) pg->bg = c | 0xFF000000u;
        JS_FreeValue(ctx, hcs);
        JS_FreeValue(ctx, cs);
    }
    l.x0 = margin; l.width = SCREEN_W_CSS - 2 * margin; l.y = margin; l.lx = l.x0;
    if (JS_IsObject(html)) layout_node(&l, html, 0);
    flush_line(&l);
    pg->doc_h = l.y + margin;
    for (int i = 0; i < l.nabs; i++) { layout_abs(&l, l.abs[i]); JS_FreeValue(ctx, l.abs[i]); }
    free(l.abs);
    /* element rects */
    for (int i = 0; i < l.npend; i++) {
        Pend *p = &l.pend[i];
        int x = p->x, y = p->y, w = p->w, h = p->h;
        if (!p->block) {
            int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
            for (int k = p->i0; k < p->i1 && k < pg->nitems; k++) {
                DItem *it = &pg->items[k];
                if (it->x < minx) minx = it->x; if (it->y < miny) miny = it->y;
                if (it->x + it->w > maxx) maxx = it->x + it->w; if (it->y + it->h > maxy) maxy = it->y + it->h;
            }
            if (minx <= maxx) { x = minx; y = miny; w = maxx - minx; h = maxy - miny; } else { x = y = w = h = 0; }
        }
        JSValue r = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, r, 0, JS_NewInt32(ctx, x)); JS_SetPropertyUint32(ctx, r, 1, JS_NewInt32(ctx, y - pg->scroll_y));
        JS_SetPropertyUint32(ctx, r, 2, JS_NewInt32(ctx, w)); JS_SetPropertyUint32(ctx, r, 3, JS_NewInt32(ctx, h));
        JS_SetPropertyStr(ctx, p->el, "__r", r);
        JS_FreeValue(ctx, p->el);
    }
    free(l.pend);
    /* link boxes */
    for (int k = 0; k < pg->nlinks; k++) {
        Link *lk = &pg->links[k];
        int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
        for (int i = 0; i < pg->nitems; i++) {
            DItem *it = &pg->items[i];
            if (it->link != k) continue;
            if (it->x < minx) minx = it->x; if (it->y < miny) miny = it->y;
            if (it->x + it->w > maxx) maxx = it->x + it->w; if (it->y + it->h > maxy) maxy = it->y + it->h;
            lk->fixed = it->fixed;
        }
        if (minx <= maxx) { lk->x = minx; lk->y = miny; lk->w = maxx - minx; lk->h = maxy - miny; }
    }
    JS_FreeValue(ctx, html); JS_FreeValue(ctx, body); JS_FreeValue(ctx, doc); JS_FreeValue(ctx, g);
    pg->dirty = 0;
    pg->laid_out = 1;
    pg->redraw = 1;
    if (pg->scroll_y > pg->doc_h - SCREEN_H_CSS) pg->scroll_y = pg->doc_h > SCREEN_H_CSS ? pg->doc_h - SCREEN_H_CSS : 0;
}

JSValue page_element_at(Page *pg, int x, int y)
{
    /* topmost canvas/image item, else link, else body */
    for (int i = pg->nitems - 1; i >= 0; i--) {
        DItem *it = &pg->items[i];
        int yy = it->fixed ? y - pg->scroll_y : y;
        if ((it->type == DI_CANVAS || it->type == DI_IMAGE) && x >= it->x && x < it->x + it->w && yy >= it->y && yy < it->y + it->h)
            return JS_DupValue(pg->ctx, it->obj);
    }
    for (int k = 0; k < pg->nlinks; k++) {
        Link *lk = &pg->links[k];
        int yy = lk->fixed ? y - pg->scroll_y : y;
        if (x >= lk->x && x < lk->x + lk->w && yy >= lk->y && yy < lk->y + lk->h) return JS_DupValue(pg->ctx, lk->el);
    }
    return JS_UNDEFINED;
}
