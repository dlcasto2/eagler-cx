/* Page = one loaded HTML document with its own JS runtime */
#include "page.h"
#include "html.h"
#include "../js/webgl_js.h"
#include "../js/canvas2d.h"
#include "../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_TGA
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static const char prelude_src[] =
#include "prelude.inc"
;

static Page *cur_page;

/* ---------------- files ---------------- */
char *load_file(const char *path, int *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc(n + 1);
    if (!b) { fclose(f); return NULL; }
    long r = fread(b, 1, n, f);
    fclose(f);
    b[r] = 0;
    if (len) *len = (int)r;
    return b;
}
static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

/* resolve url relative to page dir; tries with and without .tns; returns 1 if file exists */
int page_resolve(Page *pg, const char *url, char *out, int outsz)
{
    char u[512];
    snprintf(u, sizeof u, "%s", url);
    char *q = strpbrk(u, "?#"); if (q) *q = 0;
    const char *p = u;
    if (!strncmp(p, "file://", 7)) p += 7;
    char base[512];
    if (p[0] == '/') snprintf(base, sizeof base, "%s", p);
    else {
        while (!strncmp(p, "./", 2)) p += 2;
        snprintf(base, sizeof base, "%s%s", pg->dir, p);
    }
    /* collapse "dir/../" */
    char *d;
    while ((d = strstr(base, "/../"))) {
        char *s = d - 1;
        while (s > base && *s != '/') s--;
        if (s < base || *s != '/') break;
        memmove(s, d + 3, strlen(d + 3) + 1);
    }
    snprintf(out, outsz, "%s.tns", base);
    if (file_exists(out)) return 1;
    snprintf(out, outsz, "%s", base);
    if (file_exists(out)) return 1;
    return 0;
}

void page_log(Page *pg, int lvl, const char *fmt, ...)
{
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (lvl >= 2) pg->errors++;
    if (plat_is_host()) fprintf(stderr, "[%s] %s\n", lvl == 2 ? "ERR" : lvl == 1 ? "WARN" : "LOG", buf);
    /* split into lines */
    char *s = buf;
    while (s && *s) {
        char *nl = strchr(s, '\n');
        if (nl) *nl = 0;
        int idx = (pg->loghead + pg->logn) % LOG_LINES;
        if (pg->logn == LOG_LINES) pg->loghead = (pg->loghead + 1) % LOG_LINES; else pg->logn++;
        snprintf(pg->log[idx], sizeof pg->log[idx], "%s", s);
        pg->loglvl[idx] = lvl;
        s = nl ? nl + 1 : NULL;
    }
}

static void dump_exception(Page *pg)
{
    JSContext *ctx = pg->ctx;
    JSValue ex = JS_GetException(ctx);
    const char *s = JS_ToCString(ctx, ex);
    page_log(pg, 2, "%s", s ? s : "exception");
    JS_FreeCString(ctx, s);
    if (JS_IsError(ctx, ex)) {
        JSValue st = JS_GetPropertyStr(ctx, ex, "stack");
        if (JS_IsString(st)) { const char *t = JS_ToCString(ctx, st); if (t && *t) page_log(pg, 2, "%s", t); JS_FreeCString(ctx, t); }
        JS_FreeValue(ctx, st);
    }
    JS_FreeValue(ctx, ex);
}

void page_run_jobs(Page *pg)
{
    JSContext *c2;
    for (int i = 0; i < 10000; i++) {
        int r = JS_ExecutePendingJob(pg->rt, &c2);
        if (r <= 0) { if (r < 0) dump_exception(pg); break; }
    }
}

/* ---------------- natives ---------------- */
#define FN(name) static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
#define PG Page *pg = cur_page; (void)this_val; (void)argc;

FN(n_log)
{
    PG int32_t lvl = 0; JS_ToInt32(ctx, &lvl, argv[0]);
    const char *s = JS_ToCString(ctx, argv[1]);
    page_log(pg, lvl, "%s", s ? s : "");
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
FN(n_now) { (void)this_val; (void)argc; (void)argv; return JS_NewFloat64(ctx, (double)plat_ms()); }

static char *data_url_decode(const char *u, int *len)
{
    const char *comma = strchr(u, ',');
    if (!comma) return NULL;
    int b64 = strstr(u, ";base64") && strstr(u, ";base64") < comma;
    const char *s = comma + 1;
    int n = strlen(s);
    char *out = malloc(n + 1);
    int o = 0;
    if (b64) {
        static const char *k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        unsigned bits = 0; int nb = 0;
        for (int i = 0; i < n; i++) {
            const char *p = strchr(k, s[i]);
            if (!p || !s[i]) continue;
            bits = (bits << 6) | (p - k); nb += 6;
            if (nb >= 8) { nb -= 8; out[o++] = (bits >> nb) & 255; }
        }
    } else {
        for (int i = 0; i < n; i++) {
            if (s[i] == '%' && i + 2 < n) { char h[3] = { s[i + 1], s[i + 2], 0 }; out[o++] = (char)strtol(h, NULL, 16); i += 2; }
            else out[o++] = s[i];
        }
    }
    out[o] = 0;
    *len = o;
    return out;
}

static char *load_url(Page *pg, const char *url, int *len)
{
    if (!strncmp(url, "data:", 5)) return data_url_decode(url, len);
    char p[512];
    if (!page_resolve(pg, url, p, sizeof p)) return NULL;
    return load_file(p, len);
}

FN(n_loadText)
{
    PG const char *u = JS_ToCString(ctx, argv[0]);
    int len; char *b = u ? load_url(pg, u, &len) : NULL;
    JS_FreeCString(ctx, u);
    if (!b) return JS_NULL;
    JSValue r = JS_NewStringLen(ctx, b, len);
    free(b);
    return r;
}
FN(n_loadBinary)
{
    PG const char *u = JS_ToCString(ctx, argv[0]);
    int len; char *b = u ? load_url(pg, u, &len) : NULL;
    JS_FreeCString(ctx, u);
    if (!b) return JS_NULL;
    JSValue r = JS_NewArrayBufferCopy(ctx, (uint8_t *)b, len);
    free(b);
    return r;
}
FN(n_exists)
{
    PG const char *u = JS_ToCString(ctx, argv[0]);
    char p[512]; int ok = u && page_resolve(pg, u, p, sizeof p);
    JS_FreeCString(ctx, u);
    return JS_NewBool(ctx, ok);
}
FN(n_loadImage)
{
    PG const char *u = JS_ToCString(ctx, argv[0]);
    int len; char *b = u ? load_url(pg, u, &len) : NULL;
    if (!b) { JS_FreeCString(ctx, u); return JS_NULL; }
    int w, h, ch;
    unsigned char *px = stbi_load_from_memory((unsigned char *)b, len, &w, &h, &ch, 4);
    free(b);
    if (!px) { page_log(pg, 1, "cannot decode image %s: %s", u, stbi_failure_reason()); JS_FreeCString(ctx, u); return JS_NULL; }
    JS_FreeCString(ctx, u);
    /* downscale big images to save memory/time */
    while (w > 512 || h > 512) {
        int nw = w / 2, nh = h / 2;
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) for (int c = 0; c < 4; c++) {
            int s = px[((y * 2) * w + x * 2) * 4 + c] + px[((y * 2) * w + x * 2 + 1) * 4 + c] +
                    px[((y * 2 + 1) * w + x * 2) * 4 + c] + px[((y * 2 + 1) * w + x * 2 + 1) * 4 + c];
            px[(y * nw + x) * 4 + c] = s / 4;
        }
        w = nw; h = nh;
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, o, "height", JS_NewInt32(ctx, h));
    JS_SetPropertyStr(ctx, o, "data", JS_NewArrayBufferCopy(ctx, px, (size_t)w * h * 4));
    stbi_image_free(px);
    return o;
}
FN(n_parseHTML)
{
    (void)this_val; (void)argc;
    size_t len; const char *s = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!s) return JS_NewArray(ctx);
    JSValue r = html_parse(ctx, s, (int)len);
    JS_FreeCString(ctx, s);
    return r;
}
FN(n_createWebGL) { PG pg->dirty = 1; return webgl_create(ctx, argv[0], argc > 1 ? argv[1] : JS_UNDEFINED); }
FN(n_create2D) { PG pg->dirty = 1; return canvas2d_create(ctx, argv[0]); }
FN(n_alert)
{
    PG const char *s = JS_ToCString(ctx, argv[0]);
    page_log(pg, 0, "alert: %s", s ? s : "");
    ui_alert(s ? s : "");
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
FN(n_dirty) { PG (void)ctx; (void)argv; pg->dirty = 1; return JS_UNDEFINED; }
FN(n_navigate)
{
    PG const char *s = JS_ToCString(ctx, argv[0]);
    if (s) { snprintf(pg->nav, sizeof pg->nav, "%s", s); pg->nav_req = 1; }
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
FN(n_back) { PG (void)ctx; (void)argv; strcpy(pg->nav, "#back"); pg->nav_req = 1; return JS_UNDEFINED; }
FN(n_title)
{
    PG const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        /* trim */
        while (*s == ' ' || *s == '\n' || *s == '\t') s++;
        snprintf(pg->title, sizeof pg->title, "%s", s);
    }
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
FN(n_rect)
{
    PG if (pg->dirty || !pg->laid_out) page_layout(pg);
    JSValue r = JS_GetPropertyStr(ctx, argv[0], "__r");
    if (JS_IsUndefined(r)) return JS_NULL;
    return r;
}
FN(n_url) { PG (void)argv; return JS_NewString(ctx, pg->path); }

static const JSCFunctionListEntry natives[] = {
    JS_CFUNC_DEF("log", 2, n_log), JS_CFUNC_DEF("now", 0, n_now), JS_CFUNC_DEF("loadText", 1, n_loadText),
    JS_CFUNC_DEF("loadBinary", 1, n_loadBinary), JS_CFUNC_DEF("exists", 1, n_exists), JS_CFUNC_DEF("loadImage", 1, n_loadImage),
    JS_CFUNC_DEF("parseHTML", 1, n_parseHTML), JS_CFUNC_DEF("createWebGL", 2, n_createWebGL), JS_CFUNC_DEF("create2D", 2, n_create2D),
    JS_CFUNC_DEF("alert", 1, n_alert), JS_CFUNC_DEF("dirty", 0, n_dirty), JS_CFUNC_DEF("navigate", 1, n_navigate),
    JS_CFUNC_DEF("back", 0, n_back), JS_CFUNC_DEF("title", 1, n_title), JS_CFUNC_DEF("rect", 1, n_rect), JS_CFUNC_DEF("url", 0, n_url),
};

/* ---------------- modules ---------------- */
static JSModuleDef *module_loader(JSContext *ctx, const char *name, void *opaque)
{
    Page *pg = opaque;
    int len;
    char *src = load_file(name, &len);
    if (!src) {
        char p[512];
        if (page_resolve(pg, name, p, sizeof p)) src = load_file(p, &len);
    }
    if (!src) { JS_ThrowReferenceError(ctx, "could not load module '%s'", name); return NULL; }
    JSValue fv = JS_Eval(ctx, src, len, name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(src);
    if (JS_IsException(fv)) return NULL;
    JSModuleDef *m = JS_VALUE_GET_PTR(fv);
    JS_FreeValue(ctx, fv);
    return m;
}
static char *module_normalize(JSContext *ctx, const char *base, const char *name, void *opaque)
{
    Page *pg = opaque;
    char out[512];
    if (name[0] == '/' ) snprintf(out, sizeof out, "%s", name);
    else if (name[0] == '.') {
        char dir[512]; snprintf(dir, sizeof dir, "%s", base);
        char *s = strrchr(dir, '/'); if (s) s[1] = 0; else dir[0] = 0;
        const char *n = name;
        while (!strncmp(n, "./", 2)) n += 2;
        while (!strncmp(n, "../", 3)) { n += 3; size_t l = strlen(dir); if (l > 1) { dir[l - 1] = 0; char *t = strrchr(dir, '/'); if (t) t[1] = 0; } }
        snprintf(out, sizeof out, "%s%s", dir, n);
    } else {
        page_log(pg, 2, "bare module specifier '%s' not supported (no network); use a relative path", name);
        snprintf(out, sizeof out, "%s%s", pg->dir, name);
    }
    char res[512];
    if (page_resolve(pg, out, res, sizeof res)) snprintf(out, sizeof out, "%s", res);
    return js_strdup(ctx, out);
}

static int interrupt_handler(JSRuntime *rt, void *opaque)
{
    (void)rt;
    Page *pg = opaque;
    static int cnt;
    if (++cnt & 63) return 0;
    if (plat_on_pressed()) { pg->interrupted = 1; return 1; }
    if (pg->deadline && plat_is_host() == 0 && plat_ms() > pg->deadline + 15000) { pg->interrupted = 1; return 1; }
    return 0;
}

/* ---------------- page lifecycle ---------------- */
static JSValue call_rt(Page *pg, const char *fn, int argc, JSValueConst *argv)
{
    JSContext *ctx = pg->ctx;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue rt = JS_GetPropertyStr(ctx, g, "__rt");
    JSValue f = JS_GetPropertyStr(ctx, rt, fn);
    JSValue r = JS_Call(ctx, f, rt, argc, argv);
    if (JS_IsException(r)) dump_exception(pg);
    JS_FreeValue(ctx, f); JS_FreeValue(ctx, rt); JS_FreeValue(ctx, g);
    return r;
}

static void run_script(Page *pg, const char *src, int len, const char *name, int module)
{
    JSContext *ctx = pg->ctx;
    pg->deadline = plat_ms();
    JSValue r = JS_Eval(ctx, src, len, name, module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) dump_exception(pg);
    else if (module) {
        /* module evaluation returns a promise; surface rejection */
        page_run_jobs(pg);
        if (JS_IsObject(r)) {
            JSPromiseStateEnum st = JS_PromiseState(ctx, r);
            if (st == JS_PROMISE_REJECTED) {
                JSValue e = JS_PromiseResult(ctx, r);
                JS_Throw(ctx, e);
                dump_exception(pg);
            }
        }
    }
    JS_FreeValue(ctx, r);
    pg->deadline = 0;
    page_run_jobs(pg);
}

Page *page_open(const char *path)
{
    Page *pg = calloc(1, sizeof(Page));
    cur_page = pg;
    snprintf(pg->path, sizeof pg->path, "%s", path);
    snprintf(pg->dir, sizeof pg->dir, "%s", path);
    char *s = strrchr(pg->dir, '/'); if (s) s[1] = 0; else strcpy(pg->dir, "./");
    /* title default = file name without .tns */
    const char *fn = strrchr(path, '/'); fn = fn ? fn + 1 : path;
    snprintf(pg->title, sizeof pg->title, "%s", fn);
    char *t = strstr(pg->title, ".tns"); if (t) *t = 0;
    pg->focus_link = -1;
    pg->bg = 0xFFFFFFFF;

    pg->rt = JS_NewRuntime();
    JS_SetMemoryLimit(pg->rt, 32 * 1024 * 1024);
    JS_SetMaxStackSize(pg->rt, plat_is_host() ? 4 * 1024 * 1024 : 1536 * 1024);
    JS_SetInterruptHandler(pg->rt, interrupt_handler, pg);
    JS_SetModuleLoaderFunc(pg->rt, module_normalize, module_loader, pg);
    pg->ctx = JS_NewContext(pg->rt);
    JSContext *ctx = pg->ctx;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue n = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, n, natives, sizeof natives / sizeof natives[0]);
    JS_SetPropertyStr(ctx, g, "__n", n);
    JS_FreeValue(ctx, g);
    webgl_init(ctx);
    canvas2d_init(ctx);
    JSValue r = JS_Eval(ctx, prelude_src, strlen(prelude_src), "<prelude>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) dump_exception(pg);
    JS_FreeValue(ctx, r);

    int len;
    char *html = load_file(path, &len);
    if (!html) {
        page_log(pg, 2, "cannot open %s", path);
        html = strdup("<html><body><h1>File not found</h1></body></html>");
        len = strlen(html);
    }
    JSValue tree = html_parse(ctx, html, len);
    free(html);
    JSValue res = call_rt(pg, "build", 1, (JSValueConst *)&tree);
    JS_FreeValue(ctx, res);
    JS_FreeValue(ctx, tree);
    page_layout(pg);

    /* run scripts in document order */
    JSValue scripts = call_rt(pg, "scripts", 0, NULL);
    int32_t ns = 0;
    JS_ToInt32(ctx, &ns, JS_GetPropertyStr(ctx, scripts, "length"));
    for (int i = 0; i < ns; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, scripts, i);
        JSValue attrs = JS_GetPropertyStr(ctx, el, "_attrs");
        JSValue tv = JS_GetPropertyStr(ctx, attrs, "type"), sv = JS_GetPropertyStr(ctx, attrs, "src");
        const char *type = JS_IsString(tv) ? JS_ToCString(ctx, tv) : NULL;
        int module = type && !strcmp(type, "module");
        int run = !type || !*type || module || strstr(type, "javascript") || strstr(type, "ecmascript");
        if (run) {
            if (JS_IsString(sv)) {
                const char *src = JS_ToCString(ctx, sv);
                char p[512];
                if (src && page_resolve(pg, src, p, sizeof p)) {
                    int l; char *code = load_file(p, &l);
                    if (code) { run_script(pg, code, l, p, module); free(code); }
                } else page_log(pg, 2, "script not found: %s", src ? src : "?");
                JS_FreeCString(ctx, src);
            } else {
                JSValue txt = JS_GetPropertyStr(ctx, el, "textContent");
                size_t l; const char *code = JS_ToCStringLen(ctx, &l, txt);
                if (code) run_script(pg, code, (int)l, pg->path, module);
                JS_FreeCString(ctx, code);
                JS_FreeValue(ctx, txt);
            }
        }
        JS_FreeCString(ctx, type);
        JS_FreeValue(ctx, tv); JS_FreeValue(ctx, sv); JS_FreeValue(ctx, attrs); JS_FreeValue(ctx, el);
        if (pg->interrupted) break;
    }
    JS_FreeValue(ctx, scripts);
    res = call_rt(pg, "ready", 0, NULL);
    JS_FreeValue(ctx, res);
    page_run_jobs(pg);
    pg->dirty = 1;
    return pg;
}

void page_close(Page *pg)
{
    if (!pg) return;
    cur_page = pg;
    for (int i = 0; i < pg->nitems; i++) { free(pg->items[i].text); JS_FreeValue(pg->ctx, pg->items[i].obj); }
    for (int i = 0; i < pg->nlinks; i++) JS_FreeValue(pg->ctx, pg->links[i].el);
    free(pg->items); free(pg->links);
    webgl_free_protos(pg->ctx);
    canvas2d_free_protos(pg->ctx);
    JS_FreeContext(pg->ctx);
    JS_FreeRuntime(pg->rt);
    free(pg);
    cur_page = NULL;
}

int page_tick(Page *pg, uint32_t now)
{
    cur_page = pg;
    JSValue a = JS_NewFloat64(pg->ctx, (double)now);
    pg->deadline = now;
    JSValue r = call_rt(pg, "tick", 1, (JSValueConst *)&a);
    pg->deadline = 0;
    int pending = JS_ToBool(pg->ctx, r);
    JS_FreeValue(pg->ctx, r);
    page_run_jobs(pg);
    if (!pending) {
        JSValue nt = call_rt(pg, "nextTimer", 0, NULL);
        double t = 1e30; JS_ToFloat64(pg->ctx, &t, nt); JS_FreeValue(pg->ctx, nt);
        if (t <= now + 40) pending = 1;
    }
    return pending;
}

void page_key(Page *pg, int down, const char *key, const char *code, int keyCode, int shift, int repeat)
{
    cur_page = pg;
    JSContext *ctx = pg->ctx;
    JSValue args[6] = { JS_NewString(ctx, down ? "keydown" : "keyup"), JS_NewString(ctx, key), JS_NewString(ctx, code),
                        JS_NewInt32(ctx, keyCode), JS_NewBool(ctx, shift), JS_NewBool(ctx, repeat) };
    JSValue r = call_rt(pg, "key", 6, args);
    JS_FreeValue(ctx, r);
    if (down && strlen(key) == 1) {
        JS_FreeValue(ctx, args[0]);
        args[0] = JS_NewString(ctx, "keypress");
        r = call_rt(pg, "key", 6, args);
        JS_FreeValue(ctx, r);
    }
    for (int i = 0; i < 6; i++) JS_FreeValue(ctx, args[i]);
    page_run_jobs(pg);
}

void page_mouse(Page *pg, const char *type, int x, int y, JSValue el, int dx, int dy, int buttons)
{
    cur_page = pg;
    JSContext *ctx = pg->ctx;
    JSValue args[7] = { JS_NewString(ctx, type), JS_NewInt32(ctx, x), JS_NewInt32(ctx, y), JS_DupValue(ctx, el),
                        JS_NewInt32(ctx, dx), JS_NewInt32(ctx, dy), JS_NewInt32(ctx, buttons) };
    JSValue r = call_rt(pg, "mouse", 7, args);
    JS_FreeValue(ctx, r);
    for (int i = 0; i < 7; i++) JS_FreeValue(ctx, args[i]);
    page_run_jobs(pg);
}

int page_has_keys(Page *pg)
{
    cur_page = pg;
    JSValue r = call_rt(pg, "hasKeys", 0, NULL);
    int b = JS_ToBool(pg->ctx, r); JS_FreeValue(pg->ctx, r); return b;
}
int page_has_mouse(Page *pg)
{
    cur_page = pg;
    JSValue r = call_rt(pg, "hasMouse", 0, NULL);
    int b = JS_ToBool(pg->ctx, r); JS_FreeValue(pg->ctx, r); return b;
}
void page_set_current(Page *pg) { cur_page = pg; }
void page_call_resize(Page *pg) { JSValue r = call_rt(pg, "resize", 0, NULL); JS_FreeValue(pg->ctx, r); }
void page_compute_styles(Page *pg) { JSValue r = call_rt(pg, "styles", 0, NULL); JS_FreeValue(pg->ctx, r); }
