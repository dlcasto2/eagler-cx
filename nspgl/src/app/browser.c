/* nspGL browser shell: file picker, page view, menu, console */
#include "page.h"
#include "gfx.h"
#include "../js/webgl_js.h"
#include "../js/canvas2d.h"
#include "../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void page_set_current(Page *pg);

#define VERSION "1.0"
static uint8_t kdown[K_COUNT], kprev[K_COUNT];
static int quit_app;
static int quality = 50, show_fps, fullscreen;
static char history[16][512]; static int nhist;
static int cur_x = 160, cur_y = 120, cur_vis;
static uint32_t cur_last;

static int pressed(int k) { return kdown[k] && !kprev[k]; }
static void poll_keys(void) { memcpy(kprev, kdown, K_COUNT); plat_keys(kdown); }
static void wait_release(void)
{
    for (int i = 0; i < 200; i++) {
        plat_keys(kdown);
        int any = 0; for (int k = 1; k < K_COUNT; k++) any |= kdown[k];
        if (!any || plat_is_host()) break;
        plat_wait_ms(10);
    }
    memcpy(kprev, kdown, K_COUNT);
}

/* ---------------- small UI helpers ---------------- */
static void draw_box(int x, int y, int w, int h, const char *title)
{
    gfx_fill(x + 3, y + 3, w, h, 0x000000);
    gfx_fill(x, y, w, h, 0xF4F4F4);
    gfx_rect(x, y, w, h, 0x333333);
    if (title) {
        gfx_fill(x + 1, y + 1, w - 2, 15, 0x2B4C7E);
        gfx_text(FNT_BOLD, x + 5, y + 2, title, strlen(title), 0xFFFFFF);
    }
}
static int wrap_lines(int font, const char *s, int width, const char **starts, int *lens, int max)
{
    int n = 0;
    while (*s && n < max) {
        const char *ls = s, *last_sp = NULL;
        int w = 0;
        while (*s && *s != '\n') {
            if (*s == ' ') last_sp = s;
            w += gfx_char_width(font, (unsigned char)*s);
            if (w > width) { if (last_sp && last_sp > ls) s = last_sp; break; }
            s++;
        }
        starts[n] = ls; lens[n] = s - ls; n++;
        if (*s == ' ' || *s == '\n') s++;
    }
    return n;
}

void ui_alert(const char *msg)
{
    if (plat_is_host()) return;
    const char *st[12]; int ln[12];
    int n = wrap_lines(FNT_SANS, msg, 250, st, ln, 12);
    int h = 40 + n * 13;
    draw_box(30, 120 - h / 2, 260, h, "Alert");
    for (int i = 0; i < n; i++) gfx_text(FNT_SANS, 38, 120 - h / 2 + 20 + i * 13, st[i], ln[i], 0x000000);
    gfx_text(FNT_SMALL, 200, 120 + h / 2 - 12, "[enter] OK", 10, 0x555555);
    plat_present();
    wait_release();
    for (;;) {
        poll_keys();
        if (pressed(K_ENTER) || pressed(K_ESC) || pressed(K_RET) || pressed(K_CLICK) || plat_on_pressed()) break;
        plat_wait_ms(20);
    }
    wait_release();
}

static void status(const char *msg)
{
    gfx_noclip();
    gfx_fill(0, SCR_H - 14, SCR_W, 14, 0x2B4C7E);
    gfx_text(FNT_SANS, 4, SCR_H - 14, msg, strlen(msg), 0xFFFFFF);
    plat_present();
}

/* ---------------- file picker ---------------- */
typedef struct Ent { char name[128]; int dir; } Ent;
static int ent_cmp(const void *a, const void *b)
{
    const Ent *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return strcasecmp(x->name, y->name);
}
static int is_html(const char *n)
{
    const char *e[] = { ".html.tns", ".htm.tns", ".html", ".htm", NULL };
    size_t l = strlen(n);
    for (int i = 0; e[i]; i++) { size_t k = strlen(e[i]); if (l > k && !strcasecmp(n + l - k, e[i])) return 1; }
    return 0;
}

/* returns 1 and fills out with chosen path, 0 to quit */
static int file_picker(char *dir, char *out, int outsz)
{
    Ent *ents = NULL; int n = 0, cap = 0, sel = 0, top = 0;
    int reload = 1;
    wait_release();
    for (;;) {
        if (reload) {
            n = 0;
            DIR *d = opendir(dir);
            if (strcmp(dir, "/") && strcmp(dir, "/documents/")) {
                if (n == cap) { cap = cap ? cap * 2 : 32; ents = realloc(ents, cap * sizeof(Ent)); }
                strcpy(ents[n].name, ".."); ents[n].dir = 1; n++;
            }
            if (d) {
                struct dirent *de;
                while ((de = readdir(d))) {
                    if (de->d_name[0] == '.') continue;
                    char full[700]; snprintf(full, sizeof full, "%s%s", dir, de->d_name);
                    struct stat st;
                    int isdir = !stat(full, &st) && S_ISDIR(st.st_mode);
                    if (!isdir && !is_html(de->d_name)) continue;
                    if (n == cap) { cap = cap ? cap * 2 : 32; ents = realloc(ents, cap * sizeof(Ent)); }
                    snprintf(ents[n].name, sizeof ents[n].name, "%s", de->d_name);
                    ents[n].dir = isdir; n++;
                }
                closedir(d);
            }
            qsort(ents + (n && !strcmp(ents[0].name, "..") ? 1 : 0), n && !strcmp(ents[0].name, "..") ? n - 1 : n, sizeof(Ent), ent_cmp);
            sel = (n > 1 && !strcmp(ents[0].name, "..")) ? 1 : 0; top = 0; reload = 0;
        }
        /* draw */
        gfx_noclip();
        gfx_fill(0, 0, SCR_W, SCR_H, 0xFFFFFF);
        gfx_fill(0, 0, SCR_W, 20, 0x2B4C7E);
        gfx_text(FNT_H2, 5, 1, "nspGL", 5, 0xFFFFFF);
        char hdr[128]; snprintf(hdr, sizeof hdr, "WebGL viewer v%s  %s", VERSION, dir);
        gfx_clip(60, 0, SCR_W, 20);
        gfx_text(FNT_SMALL, 62, 6, hdr, strlen(hdr), 0xC8D8F0);
        gfx_noclip();
        int rows = (SCR_H - 20 - 16) / 14;
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        if (!n) gfx_text(FNT_SANS, 10, 30, "No .html files here.", 20, 0x666666);
        for (int i = 0; i < rows && top + i < n; i++) {
            Ent *e = &ents[top + i];
            int y = 22 + i * 14;
            if (top + i == sel) gfx_fill(0, y, SCR_W, 14, 0xCFE0FA);
            char nm[160];
            snprintf(nm, sizeof nm, "%s%s", e->name, e->dir ? "/" : "");
            char *t = strstr(nm, ".tns"); if (t && !e->dir) *t = 0;
            gfx_text(e->dir ? FNT_BOLD : FNT_SANS, 8, y, nm, strlen(nm), e->dir ? 0x2B4C7E : 0x000000);
        }
        gfx_fill(0, SCR_H - 15, SCR_W, 15, 0xE8E8E8);
        const char *help = "[enter] open  [esc] quit  [arrows] select";
        gfx_text(FNT_SMALL, 5, SCR_H - 13, help, strlen(help), 0x444444);
        plat_present();
        /* input */
        int acted = 0;
        while (!acted) {
            poll_keys();
            if (plat_on_pressed() || pressed(K_ESC)) { free(ents); return 0; }
            if (pressed(K_DOWN) && sel + 1 < n) { sel++; acted = 1; }
            if (pressed(K_UP) && sel > 0) { sel--; acted = 1; }
            if (pressed(K_RIGHT) || pressed(K_ENTER) || pressed(K_CLICK)) {
                if (n) {
                    Ent *e = &ents[sel];
                    if (!strcmp(e->name, "..")) {
                        size_t l = strlen(dir);
                        if (l > 1) { dir[l - 1] = 0; char *s = strrchr(dir, '/'); if (s) s[1] = 0; }
                        reload = 1;
                    } else if (e->dir) { strcat(dir, e->name); strcat(dir, "/"); reload = 1; }
                    else { snprintf(out, outsz, "%s%s", dir, e->name); free(ents); return 1; }
                }
                acted = 1;
            }
            if (pressed(K_LEFT) && strcmp(dir, "/")) {
                size_t l = strlen(dir);
                if (l > 1) { dir[l - 1] = 0; char *s = strrchr(dir, '/'); if (s) s[1] = 0; }
                reload = 1; acted = 1;
            }
            if (!acted) plat_wait_ms(20);
            if (plat_is_host() && !acted) { /* host: auto-open first html to keep tests moving */
                static int spins; if (++spins > 3) { free(ents); return 0; }
            }
        }
    }
}

/* ---------------- rendering ---------------- */
static uint32_t rgb(uint32_t argb) { return argb & 0xFFFFFF; }

static int canvas_px(Page *pg, JSValue el, const uint32_t **px, int *w, int *h, int *flip, int *opaque)
{
    JSValue c = JS_GetPropertyStr(pg->ctx, el, "_ctx");
    int ok = 0;
    if (JS_IsObject(c)) {
        ok = canvas_ctx_pixels(pg->ctx, c, px, w, h, flip);
        int alpha = 1;
        if (!canvas2d_is(c)) { const uint32_t *p2; int a, b; webgl_pixels(pg->ctx, c, &p2, &a, &b, &alpha); }
        *opaque = !alpha;
    }
    JS_FreeValue(pg->ctx, c);
    return ok;
}

static DItem *find_canvas(Page *pg)
{
    for (int i = 0; i < pg->nitems; i++) if (pg->items[i].type == DI_CANVAS) {
        JSValue c = JS_GetPropertyStr(pg->ctx, pg->items[i].obj, "_ctx");
        int has = JS_IsObject(c);
        JS_FreeValue(pg->ctx, c);
        if (has) return &pg->items[i];
    }
    return NULL;
}

/* fullscreen canvas placement */
static void fs_rect(DItem *it, int *x, int *y, int *w, int *h)
{
    float s = (float)SCR_W / it->w;
    if (it->h * s > SCR_H) s = (float)SCR_H / it->h;
    *w = (int)(it->w * s); *h = (int)(it->h * s);
    *x = (SCR_W - *w) / 2; *y = (SCR_H - *h) / 2;
}

static void render_page(Page *pg, int fps)
{
    gfx_noclip();
    DItem *fc = fullscreen ? find_canvas(pg) : NULL;
    if (fc) {
        gfx_fill(0, 0, SCR_W, SCR_H, 0x000000);
        const uint32_t *px; int w, h, flip, op;
        if (canvas_px(pg, fc->obj, &px, &w, &h, &flip, &op)) {
            int x, y, ww, hh; fs_rect(fc, &x, &y, &ww, &hh);
            gfx_image(px, w, h, flip, x, y, ww, hh, 1, 0);
        }
    } else {
        gfx_fill(0, 0, SCR_W, SCR_H, rgb(pg->bg));
        for (int i = 0; i < pg->nitems; i++) {
            DItem *it = &pg->items[i];
            int y = it->y - (it->fixed ? 0 : pg->scroll_y);
            if (y >= SCR_H || y + it->h < 0 || it->x >= SCR_W || it->x + it->w < 0) continue;
            switch (it->type) {
            case DI_RECT:
                if ((it->color >> 24) == 255) gfx_fill(it->x, y, it->w, it->h, rgb(it->color));
                else if (it->color >> 24) gfx_fill_a(it->x, y, it->w, it->h, rgb(it->color), it->color >> 24);
                break;
            case DI_HR: gfx_fill(it->x, y, it->w, 1, rgb(it->color)); break;
            case DI_BORDER: gfx_rect(it->x, y, it->w, it->h, rgb(it->color)); break;
            case DI_TEXT: case DI_BULLET:
                if (it->color >> 24) gfx_text(it->font, it->x, y, it->text, it->len, rgb(it->color));
                if (it->link >= 0 && it->type == DI_TEXT && it->color == 0xFF0645AD) gfx_fill(it->x, y + gfx_font_ascent(it->font) + 1, it->w, 1, rgb(it->color));
                break;
            case DI_CANVAS: {
                const uint32_t *px; int w, h, flip, op;
                if (canvas_px(pg, it->obj, &px, &w, &h, &flip, &op)) gfx_image(px, w, h, flip, it->x, y, it->w, it->h, op, 0);
                break; }
            case DI_IMAGE: {
                JSValue p = JS_GetPropertyStr(pg->ctx, it->obj, "_px");
                if (JS_IsObject(p)) {
                    size_t sz; uint8_t *b = JS_GetArrayBuffer(pg->ctx, &sz, p);
                    int32_t w = 0, h = 0;
                    JSValue a = JS_GetPropertyStr(pg->ctx, it->obj, "naturalWidth"); JS_ToInt32(pg->ctx, &w, a); JS_FreeValue(pg->ctx, a);
                    a = JS_GetPropertyStr(pg->ctx, it->obj, "naturalHeight"); JS_ToInt32(pg->ctx, &h, a); JS_FreeValue(pg->ctx, a);
                    if (b && sz >= (size_t)w * h * 4) gfx_image((const uint32_t *)b, w, h, 0, it->x, y, it->w, it->h, 0, 0);
                } else gfx_rect(it->x, y, it->w, it->h, 0xAAAAAA);
                JS_FreeValue(pg->ctx, p);
                break; }
            }
        }
        if (pg->focus_link >= 0 && pg->focus_link < pg->nlinks) {
            Link *l = &pg->links[pg->focus_link];
            gfx_rect(l->x - 1, l->y - 1 - (l->fixed ? 0 : pg->scroll_y), l->w + 2, l->h + 2, 0xFF8800);
        }
        /* scrollbar */
        if (pg->doc_h > SCR_H) {
            int bh = SCR_H * SCR_H / pg->doc_h, by = pg->scroll_y * SCR_H / pg->doc_h;
            gfx_fill_a(SCR_W - 3, by, 3, bh < 8 ? 8 : bh, 0x000000, 100);
        }
    }
    if (cur_vis) {
        static const char *arrow[] = { "X.......", "XX......", "XoX.....", "XooX....", "XoooX...", "XooooX..", "XoooooX.", "XooXXXX.", "XoX.....", "XX......", "X......." };
        for (int j = 0; j < 11; j++) for (int i = 0; i < 8; i++) {
            char c = arrow[j][i];
            if (c == '.') continue;
            gfx_fill(cur_x + i, cur_y + j, 1, 1, c == 'X' ? 0x000000 : 0xFFFFFF);
        }
    }
    if (show_fps) {
        char b[32]; snprintf(b, sizeof b, "%d fps", fps);
        gfx_fill_a(0, 0, 44, 12, 0x000000, 160);
        gfx_text(FNT_SMALL, 2, 1, b, strlen(b), 0x00FF00);
    }
    if (pg->errors) {
        gfx_fill(SCR_W - 12, 0, 12, 12, 0xCC0000);
        gfx_text(FNT_BOLD, SCR_W - 8, -1, "!", 1, 0xFFFFFF);
    }
}

/* ---------------- console ---------------- */
static void console_view(Page *pg)
{
    int off = 0;
    wait_release();
    for (;;) {
        gfx_noclip();
        gfx_fill(0, 0, SCR_W, SCR_H, 0x101418);
        gfx_fill(0, 0, SCR_W, 14, 0x2B4C7E);
        char t[64]; snprintf(t, sizeof t, "Console  (%d errors)  [esc] close", pg->errors);
        gfx_text(FNT_SANS, 4, 0, t, strlen(t), 0xFFFFFF);
        int rows = (SCR_H - 16) / 10;
        int total = pg->logn;
        int first = total - rows - off; if (first < 0) first = 0;
        for (int i = 0; i < rows && first + i < total; i++) {
            int idx = (pg->loghead + first + i) % LOG_LINES;
            uint32_t c = pg->loglvl[idx] == 2 ? 0xFF6060 : pg->loglvl[idx] == 1 ? 0xFFD040 : 0xD0D0D0;
            gfx_text(FNT_SMALL, 2, 16 + i * 10, pg->log[idx], strlen(pg->log[idx]), c);
        }
        plat_present();
        int acted = 0;
        while (!acted) {
            poll_keys();
            if (pressed(K_ESC) || pressed(K_DOC) || plat_on_pressed() || plat_is_host()) { wait_release(); return; }
            if (pressed(K_UP) && off + rows < total) { off++; acted = 1; }
            if (pressed(K_DOWN) && off > 0) { off--; acted = 1; }
            if (!acted) plat_wait_ms(20);
        }
    }
}

/* ---------------- menu ---------------- */
enum { M_BACK, M_RELOAD, M_QUALITY, M_FULL, M_FPS, M_CONSOLE, M_OPEN, M_QUIT, M_N };
static int menu(Page *pg)
{
    int sel = 0;
    wait_release();
    for (;;) {
        char items[M_N][48];
        snprintf(items[M_BACK], 48, "Back");
        snprintf(items[M_RELOAD], 48, "Reload page");
        snprintf(items[M_QUALITY], 48, "3D quality: %d%%  (reloads)", quality);
        snprintf(items[M_FULL], 48, "Fullscreen canvas: %s", fullscreen ? "on" : "off");
        snprintf(items[M_FPS], 48, "Show FPS: %s", show_fps ? "on" : "off");
        snprintf(items[M_CONSOLE], 48, "Console (%d errors)", pg->errors);
        snprintf(items[M_OPEN], 48, "Open file...");
        snprintf(items[M_QUIT], 48, "Quit nspGL");
        int w = 200, h = 20 + M_N * 15, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
        render_page(pg, 0);
        draw_box(x, y, w, h, pg->title[0] ? pg->title : "nspGL");
        for (int i = 0; i < M_N; i++) {
            if (i == sel) gfx_fill(x + 2, y + 18 + i * 15, w - 4, 15, 0xCFE0FA);
            gfx_text(FNT_SANS, x + 8, y + 19 + i * 15, items[i], strlen(items[i]), 0x000000);
        }
        plat_present();
        int acted = 0;
        while (!acted) {
            poll_keys();
            if (pressed(K_ESC) || pressed(K_MENU) || plat_on_pressed()) { wait_release(); return -1; }
            if (pressed(K_DOWN)) { sel = (sel + 1) % M_N; acted = 1; }
            if (pressed(K_UP)) { sel = (sel + M_N - 1) % M_N; acted = 1; }
            if (pressed(K_ENTER) || pressed(K_CLICK) || pressed(K_RET)) {
                if (sel == M_QUALITY) { quality = quality >= 100 ? 25 : quality + 25; webgl_set_quality(quality); wait_release(); return M_RELOAD; }
                if (sel == M_FULL) { fullscreen = !fullscreen; acted = 1; continue; }
                if (sel == M_FPS) { show_fps = !show_fps; acted = 1; continue; }
                wait_release();
                return sel;
            }
            if (!acted) plat_wait_ms(20);
        }
    }
}

/* ---------------- key mapping ---------------- */
typedef struct KMap { int k; const char *key, *code; int kc; } KMap;
static const KMap kmap[] = {
    {K_UP,"ArrowUp","ArrowUp",38},{K_DOWN,"ArrowDown","ArrowDown",40},{K_LEFT,"ArrowLeft","ArrowLeft",37},
    {K_RIGHT,"ArrowRight","ArrowRight",39},{K_ENTER,"Enter","Enter",13},{K_RET,"Enter","NumpadEnter",13},
    {K_TAB,"Tab","Tab",9},{K_DEL,"Backspace","Backspace",8},{K_SPACE," ","Space",32},{K_SHIFT,"Shift","ShiftLeft",16},
    {K_CTRL,"Control","ControlLeft",17},{K_PLUS,"+","NumpadAdd",107},{K_MINUS,"-","Minus",189},{K_MUL,"*","NumpadMultiply",106},
    {K_DIV,"/","Slash",191},{K_COMMA,",","Comma",188},{K_PERIOD,".","Period",190},{K_EQU,"=","Equal",187},
    {K_LP,"(","Digit9",57},{K_RP,")","Digit0",48},{K_NEG,"-","Minus",189},{K_HOME,"Home","Home",36},{0,NULL,NULL,0}};

static void send_key(Page *pg, int k, int down)
{
    char key[8], code[16];
    int shift = kdown[K_SHIFT];
    if (k >= K_A && k <= K_Z) {
        key[0] = (shift ? 'A' : 'a') + (k - K_A); key[1] = 0;
        snprintf(code, sizeof code, "Key%c", 'A' + (k - K_A));
        page_key(pg, down, key, code, 'A' + (k - K_A), shift, 0);
        return;
    }
    if (k >= K_0 && k <= K_9) {
        key[0] = '0' + (k - K_0); key[1] = 0;
        snprintf(code, sizeof code, "Digit%d", k - K_0);
        page_key(pg, down, key, code, '0' + (k - K_0), shift, 0);
        return;
    }
    for (int i = 0; kmap[i].key; i++) if (kmap[i].k == k) { page_key(pg, down, kmap[i].key, kmap[i].code, kmap[i].kc, shift, 0); return; }
}

/* ---------------- page view ---------------- */
static int resolve_nav(Page *pg, const char *url, char *out, int sz)
{
    if (url[0] == '#') return 0;
    return page_resolve(pg, url, out, sz);
}

static Page *load_page(const char *path)
{
    gfx_noclip();
    char msg[300];
    const char *fn = strrchr(path, '/'); fn = fn ? fn + 1 : path;
    snprintf(msg, sizeof msg, "Loading %s ...", fn);
    status(msg);
    return page_open(path);
}

/* returns 0 = quit app, 1 = go to file picker */
static int view(const char *start)
{
    char path[512]; snprintf(path, sizeof path, "%s", start);
    Page *pg = load_page(path);
    uint32_t fps_t = plat_ms(); int frames = 0, fps = 0;
    wait_release();
    int result = 1;
    for (;;) {
        poll_keys();
        if (plat_on_pressed()) { result = 0; break; }
        int haskeys = page_has_keys(pg);
        int hasmouse = page_has_mouse(pg);
        int action = -1;
        if (pressed(K_ESC)) action = M_BACK;
        if (pressed(K_MENU)) action = menu(pg);
        if (pressed(K_DOC)) { console_view(pg); pg->redraw = 1; }
        if (action == M_BACK) {
            if (nhist > 0) { nhist--; snprintf(pg->nav, sizeof pg->nav, "%s", history[nhist]); pg->nav_req = 2; }
            else { result = 1; break; }
        } else if (action == M_RELOAD) { snprintf(pg->nav, sizeof pg->nav, "%s", path); pg->nav_req = 2; }
        else if (action == M_CONSOLE) { console_view(pg); pg->redraw = 1; }
        else if (action == M_OPEN) { result = 1; break; }
        else if (action == M_QUIT) { result = 0; break; }
        else if (action == M_FULL || action == M_FPS) pg->redraw = 1;
        if (action >= 0) pg->redraw = 1;

        /* keys to page */
        int ctrl = kdown[K_CTRL];
        for (int k = 1; k < K_COUNT; k++) {
            if (k == K_ESC || k == K_MENU || k == K_DOC || k == K_CLICK) continue;
            if (kdown[k] == kprev[k]) continue;
            int down = kdown[k];
            int scroll_key = (k == K_UP || k == K_DOWN) && (!haskeys || ctrl);
            if (!scroll_key && !(k == K_TAB && !haskeys) && !((k == K_ENTER || k == K_RET) && pg->focus_link >= 0 && !haskeys))
                send_key(pg, k, down);
            if (down && k == K_TAB && !haskeys && pg->nlinks) {
                pg->focus_link = (pg->focus_link + 1) % pg->nlinks;
                Link *l = &pg->links[pg->focus_link];
                if (!l->fixed) {
                    if (l->y < pg->scroll_y) pg->scroll_y = l->y - 10;
                    if (l->y + l->h > pg->scroll_y + SCR_H) pg->scroll_y = l->y + l->h - SCR_H + 10;
                }
                pg->redraw = 1;
            }
            if (down && (k == K_ENTER || k == K_RET) && pg->focus_link >= 0 && pg->focus_link < pg->nlinks && !haskeys) {
                Link *l = &pg->links[pg->focus_link];
                page_mouse(pg, "click", l->x + 1, l->y + 1 - pg->scroll_y, l->el, 0, 0, 0);
            }
        }
        /* scrolling */
        {
            int maxs = pg->doc_h - SCR_H; if (maxs < 0) maxs = 0;
            int ds = 0;
            if ((!haskeys || ctrl) && kdown[K_DOWN]) ds = kprev[K_DOWN] ? 12 : 6;
            if ((!haskeys || ctrl) && kdown[K_UP]) ds = kprev[K_UP] ? -12 : -6;
            if (ds) {
                int ns = pg->scroll_y + ds; if (ns < 0) ns = 0; if (ns > maxs) ns = maxs;
                if (ns != pg->scroll_y) { pg->scroll_y = ns; pg->redraw = 1; }
            }
        }
        /* touchpad cursor */
        int dx, dy;
        if (plat_touch(&dx, &dy) || pressed(K_CLICK)) { cur_vis = 1; cur_last = plat_ms(); }
        if (dx || dy) {
            cur_x += dx; cur_y += dy;
            if (cur_x < 0) cur_x = 0; if (cur_x > SCR_W - 1) cur_x = SCR_W - 1;
            if (cur_y < 0) cur_y = 0; if (cur_y > SCR_H - 1) cur_y = SCR_H - 1;
            pg->redraw = 1;
        }
        int mx = cur_x, my = cur_y + pg->scroll_y;
        DItem *fc = fullscreen ? find_canvas(pg) : NULL;
        if (fc) {   /* map screen -> canvas element coords */
            int x, y, w, h; fs_rect(fc, &x, &y, &w, &h);
            mx = fc->x + (cur_x - x) * fc->w / (w ? w : 1);
            my = fc->y + (cur_y - y) * fc->h / (h ? h : 1);
        }
        if ((dx || dy) && hasmouse) {
            JSValue el = page_element_at(pg, mx, my);
            page_mouse(pg, "pointermove", mx, my - pg->scroll_y, el, dx, dy, kdown[K_CLICK]);
            page_mouse(pg, "mousemove", mx, my - pg->scroll_y, el, dx, dy, kdown[K_CLICK]);
            JS_FreeValue(pg->ctx, el);
        }
        if (kdown[K_CLICK] != kprev[K_CLICK]) {
            JSValue el = page_element_at(pg, mx, my);
            if (kdown[K_CLICK]) { page_mouse(pg, "pointerdown", mx, my - pg->scroll_y, el, 0, 0, 1); page_mouse(pg, "mousedown", mx, my - pg->scroll_y, el, 0, 0, 1); }
            else { page_mouse(pg, "pointerup", mx, my - pg->scroll_y, el, 0, 0, 0); page_mouse(pg, "mouseup", mx, my - pg->scroll_y, el, 0, 0, 0); page_mouse(pg, "click", mx, my - pg->scroll_y, el, 0, 0, 0); }
            JS_FreeValue(pg->ctx, el);
        }
        if (cur_vis && plat_ms() - cur_last > 4000) { cur_vis = 0; pg->redraw = 1; }

        uint32_t now = plat_ms();
        int anim = page_tick(pg, now);
        if (pg->interrupted) {
            page_log(pg, 2, "script interrupted (ON key or time limit)");
            pg->interrupted = 0;
        }
        if (pg->nav_req) {
            char np[512];
            int ok;
            if (pg->nav_req == 2) { snprintf(np, sizeof np, "%s", pg->nav); ok = 1; }
            else if (!strcmp(pg->nav, "#back")) { ok = nhist > 0; if (ok) { nhist--; snprintf(np, sizeof np, "%s", history[nhist]); } }
            else {
                ok = resolve_nav(pg, pg->nav, np, sizeof np);
                if (ok && nhist < 16 && strcmp(np, path)) snprintf(history[nhist++], 512, "%s", path);
                if (!ok && pg->nav[0] != '#') page_log(pg, 2, "page not found: %s", pg->nav);
            }
            pg->nav_req = 0;
            if (ok) {
                page_close(pg);
                snprintf(path, sizeof path, "%s", np);
                pg = load_page(path);
                wait_release();
                continue;
            }
        }
        if (pg->dirty) page_layout(pg);
        if (anim || pg->redraw || plat_is_host()) {
            render_page(pg, fps);
            plat_present();
            pg->redraw = 0;
            frames++;
        } else plat_wait_ms(15);
        if (now - fps_t >= 1000) { fps = frames * 1000 / (now - fps_t); frames = 0; fps_t = now; if (show_fps) pg->redraw = 1; }
        if (plat_is_host()) { static int idle; if (!anim) idle++; }
    }
    page_close(pg);
    return result;
}

static void app_main(void)
{
    gfx_set_fb(plat_fb());
    webgl_set_quality(quality);
    char dir[512];
    snprintf(dir, sizeof dir, "%s", plat_start_dir());
    const char *arg = plat_arg_file();
    char path[512];
    if (arg) {
        if (!view(arg)) return;
        if (plat_is_host()) return;
    }
    while (!quit_app) {
        if (!file_picker(dir, path, sizeof path)) break;
        nhist = 0;
        if (!view(path)) break;
    }
}

int main(int argc, char **argv)
{
    if (plat_init(argc, argv)) return 1;
    plat_run(app_main);
    plat_quit();
    return 0;
}
