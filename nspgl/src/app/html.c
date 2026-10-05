/* Forgiving HTML tokenizer + tree builder producing a nested JS array tree:
 *   element = [tag, {attrs}, child...], text = "string", comment = {c:"..."} */
#include "html.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const struct { const char *n; unsigned cp; } ents[] = {
    {"amp",'&'},{"lt",'<'},{"gt",'>'},{"quot",'"'},{"apos",'\''},{"nbsp",0xA0},{"copy",0xA9},{"reg",0xAE},
    {"trade",0x2122},{"hellip",0x2026},{"mdash",0x2014},{"ndash",0x2013},{"laquo",0xAB},{"raquo",0xBB},
    {"middot",0xB7},{"bull",0x2022},{"deg",0xB0},{"plusmn",0xB1},{"times",0xD7},{"divide",0xF7},{"euro",0x20AC},
    {"pound",0xA3},{"yen",0xA5},{"cent",0xA2},{"sect",0xA7},{"para",0xB6},{"frac12",0xBD},{"frac14",0xBC},
    {"frac34",0xBE},{"sup2",0xB2},{"sup3",0xB3},{"micro",0xB5},{"larr",0x2190},{"rarr",0x2192},{"uarr",0x2191},
    {"darr",0x2193},{"harr",0x2194},{"hearts",0x2665},{"lsquo",0x2018},{"rsquo",0x2019},{"ldquo",0x201C},
    {"rdquo",0x201D},{"pi",0x3C0},{"alpha",0x3B1},{"beta",0x3B2},{"theta",0x3B8},{"lambda",0x3BB},{"mu",0x3BC},
    {"sigma",0x3C3},{"omega",0x3C9},{"Delta",0x394},{"infin",0x221E},{"ne",0x2260},{"le",0x2264},{"ge",0x2265},
    {"eacute",0xE9},{"egrave",0xE8},{"agrave",0xE0},{"aacute",0xE1},{"ouml",0xF6},{"uuml",0xFC},{"auml",0xE4},
    {"szlig",0xDF},{"ccedil",0xE7},{"ntilde",0xF1},{"iexcl",0xA1},{"iquest",0xBF},{"shy",0xAD},{"ensp",0x2002},
    {"emsp",0x2003},{"thinsp",0x2009},{"zwj",0x200D},{"zwnj",0x200C},{"check",0x2713},{NULL,0}};

typedef struct Buf { char *p; int n, cap; } Buf;
static void bput(Buf *b, const char *s, int n)
{
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0;
}
static void bputc(Buf *b, unsigned cp)
{
    char u[4]; int n;
    if (cp < 0x80) { u[0] = cp; n = 1; }
    else if (cp < 0x800) { u[0] = 0xC0 | (cp >> 6); u[1] = 0x80 | (cp & 63); n = 2; }
    else if (cp < 0x10000) { u[0] = 0xE0 | (cp >> 12); u[1] = 0x80 | ((cp >> 6) & 63); u[2] = 0x80 | (cp & 63); n = 3; }
    else { u[0] = 0xF0 | (cp >> 18); u[1] = 0x80 | ((cp >> 12) & 63); u[2] = 0x80 | ((cp >> 6) & 63); u[3] = 0x80 | (cp & 63); n = 4; }
    bput(b, u, n);
}
/* decode entities from s[0..n) into b */
static void decode(Buf *b, const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        if (s[i] != '&') { bput(b, s + i, 1); continue; }
        int j = i + 1;
        if (j < n && s[j] == '#') {
            unsigned cp = 0; j++;
            int hex = j < n && (s[j] == 'x' || s[j] == 'X');
            if (hex) j++;
            int st = j;
            while (j < n && (hex ? isxdigit((unsigned char)s[j]) : isdigit((unsigned char)s[j]))) {
                cp = cp * (hex ? 16 : 10) + (isdigit((unsigned char)s[j]) ? s[j] - '0' : (tolower((unsigned char)s[j]) - 'a' + 10));
                j++;
            }
            if (j > st) { if (j < n && s[j] == ';') j++; bputc(b, cp ? cp : 0xFFFD); i = j - 1; continue; }
        } else {
            int st = j;
            while (j < n && isalnum((unsigned char)s[j]) && j - st < 10) j++;
            for (int k = 0; ents[k].n; k++) {
                int l = strlen(ents[k].n);
                if (l == j - st && !strncmp(s + st, ents[k].n, l)) {
                    if (j < n && s[j] == ';') j++;
                    bputc(b, ents[k].cp); i = j - 1; goto next;
                }
            }
        }
        bput(b, "&", 1);
    next:;
    }
}

static int is_void(const char *t)
{
    static const char *v[] = {"br","img","hr","input","meta","link","area","base","col","embed","param","source","track","wbr",NULL};
    for (int i = 0; v[i]; i++) if (!strcmp(t, v[i])) return 1;
    return 0;
}
static int is_raw(const char *t)
{
    return !strcmp(t, "script") || !strcmp(t, "style") || !strcmp(t, "textarea") || !strcmp(t, "title") || !strcmp(t, "xmp");
}
static int closes_p(const char *t)
{
    static const char *v[] = {"address","article","aside","blockquote","div","dl","fieldset","footer","form","h1","h2","h3","h4","h5","h6",
                              "header","hr","menu","nav","ol","p","pre","section","table","ul","figure","main","details","canvas",NULL};
    for (int i = 0; v[i]; i++) if (!strcmp(t, v[i])) return 1;
    return 0;
}

#define MAXDEPTH 256
typedef struct TB {
    JSContext *ctx;
    JSValue stack[MAXDEPTH];
    char tags[MAXDEPTH][16];
    int sp;
    JSValue root;   /* JS array of top-level nodes */
} TB;

static void append(TB *tb, JSValue node)
{
    JSContext *ctx = tb->ctx;
    JSValue parent = tb->sp ? tb->stack[tb->sp - 1] : tb->root;
    JSValue lenv = JS_GetPropertyStr(ctx, parent, "length");
    int32_t len = 0; JS_ToInt32(ctx, &len, lenv); JS_FreeValue(ctx, lenv);
    JS_SetPropertyUint32(ctx, parent, len, node);
}

static void push_el(TB *tb, const char *tag, JSValue attrs)
{
    JSContext *ctx = tb->ctx;
    JSValue a = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, a, 0, JS_NewString(ctx, tag));
    JS_SetPropertyUint32(ctx, a, 1, attrs);
    append(tb, JS_DupValue(ctx, a));
    if (is_void(tag) || tb->sp >= MAXDEPTH) { JS_FreeValue(ctx, a); return; }
    tb->stack[tb->sp] = a;
    snprintf(tb->tags[tb->sp], 16, "%s", tag);
    tb->sp++;
}
static void pop_to(TB *tb, int idx)
{
    while (tb->sp > idx) { tb->sp--; JS_FreeValue(tb->ctx, tb->stack[tb->sp]); }
}
static int find_open(TB *tb, const char *tag, const char *const *stop)
{
    for (int i = tb->sp - 1; i >= 0; i--) {
        if (!strcmp(tb->tags[i], tag)) return i;
        for (int k = 0; stop && stop[k]; k++) if (!strcmp(tb->tags[i], stop[k])) return -1;
    }
    return -1;
}

static void text(TB *tb, const char *s, int n)
{
    if (n <= 0) return;
    Buf b = {0};
    decode(&b, s, n);
    if (b.n) append(tb, JS_NewStringLen(tb->ctx, b.p, b.n));
    free(b.p);
}

JSValue html_parse(JSContext *ctx, const char *src, int len)
{
    TB tb; memset(&tb, 0, sizeof tb);
    tb.ctx = ctx;
    tb.root = JS_NewArray(ctx);
    int i = 0, ts = 0;
    static const char *const stop_list[] = {"ul","ol","menu",NULL};
    static const char *const stop_tr[] = {"table","tbody","thead","tfoot",NULL};
    static const char *const stop_td[] = {"tr","table",NULL};
    static const char *const stop_dl[] = {"dl",NULL};
    static const char *const stop_p[] = {"div","td","th","li","blockquote","section","article","body","button",NULL};
    while (i < len) {
        if (src[i] != '<') { i++; continue; }
        /* flush text */
        text(&tb, src + ts, i - ts);
        if (!strncmp(src + i, "<!--", 4)) {
            const char *e = strstr(src + i + 4, "-->");
            int end = e ? (int)(e - src) : len;
            JSValue c = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, c, "c", JS_NewStringLen(ctx, src + i + 4, end - i - 4 > 0 ? end - i - 4 : 0));
            append(&tb, c);
            i = e ? end + 3 : len; ts = i; continue;
        }
        if (src[i + 1] == '!' || src[i + 1] == '?') {
            while (i < len && src[i] != '>') i++;
            i++; ts = i; continue;
        }
        int closing = src[i + 1] == '/';
        int j = i + 1 + closing;
        if (j >= len || !isalpha((unsigned char)src[j])) { i++; continue; }   /* literal '<' */
        char tag[16]; int tn = 0;
        while (j < len && (isalnum((unsigned char)src[j]) || src[j] == '-' || src[j] == ':')) { if (tn < 15) tag[tn++] = tolower((unsigned char)src[j]); j++; }
        tag[tn] = 0;
        JSValue attrs = JS_NewObject(ctx);
        int selfclose = 0;
        /* attributes */
        while (j < len && src[j] != '>') {
            if (isspace((unsigned char)src[j])) { j++; continue; }
            if (src[j] == '/') { selfclose = 1; j++; continue; }
            int ks = j;
            while (j < len && !isspace((unsigned char)src[j]) && src[j] != '=' && src[j] != '>' && !(src[j] == '/' && src[j + 1] == '>')) j++;
            char key[64]; int kn = j - ks < 63 ? j - ks : 63;
            for (int k = 0; k < kn; k++) key[k] = tolower((unsigned char)src[ks + k]);
            key[kn] = 0;
            while (j < len && isspace((unsigned char)src[j])) j++;
            Buf v = {0};
            if (j < len && src[j] == '=') {
                j++;
                while (j < len && isspace((unsigned char)src[j])) j++;
                if (src[j] == '"' || src[j] == '\'') {
                    char q = src[j++]; int vs = j;
                    while (j < len && src[j] != q) j++;
                    decode(&v, src + vs, j - vs);
                    if (j < len) j++;
                } else {
                    int vs = j;
                    while (j < len && !isspace((unsigned char)src[j]) && src[j] != '>') j++;
                    decode(&v, src + vs, j - vs);
                }
            }
            if (kn && !closing) JS_SetPropertyStr(ctx, attrs, key, JS_NewStringLen(ctx, v.p ? v.p : "", v.n));
            free(v.p);
            if (kn == 0) j++;
        }
        i = j + 1; ts = i;
        if (closing) {
            JS_FreeValue(ctx, attrs);
            int idx = find_open(&tb, tag, NULL);
            if (!strcmp(tag, "p") && idx < 0) { push_el(&tb, "p", JS_NewObject(ctx)); pop_to(&tb, tb.sp - 1); continue; }
            if (idx >= 0) pop_to(&tb, idx);
            continue;
        }
        /* implicit closes */
        if (closes_p(tag)) { int k = find_open(&tb, "p", stop_p); if (k >= 0) pop_to(&tb, k); }
        if (!strcmp(tag, "li")) { int k = find_open(&tb, "li", stop_list); if (k >= 0) pop_to(&tb, k); }
        if (!strcmp(tag, "dt") || !strcmp(tag, "dd")) {
            int k = find_open(&tb, "dt", stop_dl); if (k >= 0) pop_to(&tb, k);
            k = find_open(&tb, "dd", stop_dl); if (k >= 0) pop_to(&tb, k);
        }
        if (!strcmp(tag, "tr")) { int k = find_open(&tb, "tr", stop_tr); if (k >= 0) pop_to(&tb, k); }
        if (!strcmp(tag, "td") || !strcmp(tag, "th")) {
            int k = find_open(&tb, "td", stop_td); if (k >= 0) pop_to(&tb, k);
            k = find_open(&tb, "th", stop_td); if (k >= 0) pop_to(&tb, k);
        }
        if (!strcmp(tag, "option")) { int k = find_open(&tb, "option", NULL); if (k >= 0) pop_to(&tb, k); }
        push_el(&tb, tag, attrs);
        if (selfclose && !is_void(tag) && tb.sp && !strcmp(tb.tags[tb.sp - 1], tag) && strcmp(tag, "script")) pop_to(&tb, tb.sp - 1);
        if (is_raw(tag) && !(selfclose && strcmp(tag, "script"))) {
            /* raw text until </tag */
            int k = i;
            while (k < len) {
                if (src[k] == '<' && src[k + 1] == '/' && !strncasecmp(src + k + 2, tag, tn)) break;
                k++;
            }
            if (!strcmp(tag, "script") || !strcmp(tag, "style") || !strcmp(tag, "xmp")) {
                if (k > i) append(&tb, JS_NewStringLen(ctx, src + i, k - i));
            } else text(&tb, src + i, k - i);
            /* skip closing tag */
            while (k < len && src[k] != '>') k++;
            i = k + 1; ts = i;
            if (tb.sp && !strcmp(tb.tags[tb.sp - 1], tag)) pop_to(&tb, tb.sp - 1);
        }
    }
    if (ts < len) text(&tb, src + ts, len - ts);
    pop_to(&tb, 0);
    return tb.root;
}
