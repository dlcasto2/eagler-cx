/* GLSL preprocessor + lexer */
#include "glsl_int.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* ---------------- arena ---------------- */
void *ar_alloc(Arena *a, size_t n)
{
    n = (n + 7) & ~(size_t)7;
    if (!a->head || a->head->used + n > a->head->cap) {
        size_t cap = n > 65536 ? n : 65536;
        ABlock *b = malloc(sizeof(ABlock) + cap);
        if (!b) return NULL;
        b->next = a->head; b->used = 0; b->cap = cap;
        a->head = b;
    }
    void *p = a->head->data + a->head->used;
    a->head->used += n;
    memset(p, 0, n);
    return p;
}
char *ar_strdup(Arena *a, const char *s, int n)
{
    if (n < 0) n = strlen(s);
    char *d = ar_alloc(a, n + 1);
    memcpy(d, s, n); d[n] = 0;
    return d;
}
void ar_free(Arena *a)
{
    ABlock *b = a->head;
    while (b) { ABlock *n = b->next; free(b); b = n; }
    a->head = NULL;
}

void tv_push(TokVec *tv, Tok t)
{
    if (tv->n == tv->cap) {
        tv->cap = tv->cap ? tv->cap * 2 : 256;
        tv->v = realloc(tv->v, tv->cap * sizeof(Tok));
    }
    tv->v[tv->n++] = t;
}

void clog_append(Compiler *C, const char *fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof buf - 1) n = sizeof buf - 1;
    if (C->loglen + n + 1 > C->logcap) {
        C->logcap = (C->loglen + n + 1) * 2;
        C->log = realloc(C->log, C->logcap);
    }
    memcpy(C->log + C->loglen, buf, n + 1);
    C->loglen += n;
}

void cerr(Compiler *C, int line, const char *fmt, ...)
{
    char buf[400];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    clog_append(C, "ERROR: 0:%d: %s\n", line, buf);
    C->errors++;
    longjmp(C->jb, 1);
}

/* ---------------- lexer ---------------- */
static const char *ops3[] = { "<<=", ">>=", NULL };
static const char *ops2[] = { "++", "--", "+=", "-=", "*=", "/=", "%=", "==", "!=", "<=", ">=",
                              "&&", "||", "^^", "<<", ">>", "&=", "|=", "^=", "##", NULL };

/* lex one line of text into tokens */
static void lex_line(Compiler *C, const char *p, int line, TokVec *out)
{
    while (*p) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        Tok t; memset(&t, 0, sizeof t); t.line = line;
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *s = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            t.t = TK_IDENT; t.s = ar_strdup(&C->ar, s, p - s);
        } else if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
            const char *s = p;
            t.t = TK_NUM;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                t.num = (double)strtoul(p, (char **)&p, 16);
            } else {
                int isf = 0;
                while (isdigit((unsigned char)*p)) p++;
                if (*p == '.') { isf = 1; p++; while (isdigit((unsigned char)*p)) p++; }
                if (*p == 'e' || *p == 'E') {
                    const char *q = p + 1;
                    if (*q == '+' || *q == '-') q++;
                    if (isdigit((unsigned char)*q)) { isf = 1; p = q; while (isdigit((unsigned char)*p)) p++; }
                }
                char tmp[64]; int n = p - s; if (n > 63) n = 63;
                memcpy(tmp, s, n); tmp[n] = 0;
                if (isf) t.num = strtod(tmp, NULL);
                else if (n > 1 && tmp[0] == '0') t.num = (double)strtoul(tmp, NULL, 8);
                else t.num = strtod(tmp, NULL);
                t.isfloat = isf;
                if (*p == 'f' || *p == 'F') { t.isfloat = 1; p++; }
                else if (*p == 'u' || *p == 'U') p++;
            }
            t.s = ar_strdup(&C->ar, s, p - s);
        } else {
            int i, found = 0;
            for (i = 0; ops3[i]; i++) if (!strncmp(p, ops3[i], 3)) { t.s = ops3[i]; p += 3; found = 1; break; }
            if (!found) for (i = 0; ops2[i]; i++) if (!strncmp(p, ops2[i], 2)) { t.s = ops2[i]; p += 2; found = 1; break; }
            if (!found) { t.s = ar_strdup(&C->ar, p, 1); p++; }
            t.t = TK_OP;
        }
        tv_push(out, t);
    }
}

/* ---------------- macros ---------------- */
static struct Macro *find_macro(Compiler *C, const char *name)
{
    for (int i = C->nmacros - 1; i >= 0; i--)
        if (C->macros[i].name && !strcmp(C->macros[i].name, name)) return &C->macros[i];
    return NULL;
}
static void undef_macro(Compiler *C, const char *name)
{
    struct Macro *m = find_macro(C, name);
    if (m) m->name = NULL;
}
static void add_macro(Compiler *C, struct Macro m)
{
    undef_macro(C, m.name);
    if (C->nmacros == C->capmacros) {
        C->capmacros = C->capmacros ? C->capmacros * 2 : 32;
        C->macros = realloc(C->macros, C->capmacros * sizeof(struct Macro));
    }
    C->macros[C->nmacros++] = m;
}

static int tok_is(const Tok *t, const char *s) { return t->t == TK_OP && !strcmp(t->s, s); }

#define MAX_DISABLED 64
static void expand(Compiler *C, const Tok *in, int n, TokVec *out, const char **disabled, int ndis, int depth)
{
    if (depth > 48) cerr(C, n ? in[0].line : 0, "macro expansion too deep");
    for (int i = 0; i < n; i++) {
        const Tok *t = &in[i];
        struct Macro *m = NULL;
        if (t->t == TK_IDENT) {
            m = find_macro(C, t->s);
            for (int j = 0; m && j < ndis; j++) if (!strcmp(disabled[j], t->s)) m = NULL;
        }
        if (!m) { tv_push(out, *t); continue; }
        if (!strcmp(m->name, "__LINE__")) { Tok nt = *t; nt.t = TK_NUM; nt.num = t->line; nt.s = "0"; tv_push(out, nt); continue; }
        TokVec body = {0};
        if (m->funclike) {
            if (i + 1 >= n || !tok_is(&in[i + 1], "(")) { tv_push(out, *t); continue; }
            /* collect args */
            int argstart[17], argend[17], na = 0, lvl = 0, j = i + 2;
            argstart[0] = j;
            for (; j < n; j++) {
                if (tok_is(&in[j], "(")) lvl++;
                else if (tok_is(&in[j], ")")) { if (lvl == 0) break; lvl--; }
                else if (tok_is(&in[j], ",") && lvl == 0) {
                    if (na < 16) { argend[na] = j; na++; argstart[na] = j + 1; }
                }
            }
            if (j >= n) cerr(C, t->line, "unterminated macro call %s", m->name);
            argend[na] = j;
            if (!(na == 0 && argend[0] == argstart[0])) na++;
            if (na != m->nparams && !(m->nparams == 0 && na == 0))
                cerr(C, t->line, "macro %s expects %d arguments", m->name, m->nparams);
            /* pre-expand args */
            TokVec args[16]; memset(args, 0, sizeof args);
            for (int a = 0; a < na; a++)
                expand(C, in + argstart[a], argend[a] - argstart[a], &args[a], disabled, ndis, depth + 1);
            for (int b = 0; b < m->nbody; b++) {
                const Tok *bt = &m->body[b];
                int pi = -1;
                if (bt->t == TK_IDENT)
                    for (int p = 0; p < m->nparams; p++) if (!strcmp(m->params[p], bt->s)) { pi = p; break; }
                if (pi >= 0) { for (int k = 0; k < args[pi].n; k++) { Tok x = args[pi].v[k]; x.line = t->line; tv_push(&body, x); } }
                else { Tok x = *bt; x.line = t->line; tv_push(&body, x); }
            }
            for (int a = 0; a < na; a++) free(args[a].v);
            i = j;
        } else {
            for (int b = 0; b < m->nbody; b++) { Tok x = m->body[b]; x.line = t->line; tv_push(&body, x); }
        }
        const char *dis2[MAX_DISABLED];
        int nd2 = ndis < MAX_DISABLED - 1 ? ndis : MAX_DISABLED - 1;
        memcpy(dis2, disabled, nd2 * sizeof(char *));
        dis2[nd2++] = m->name;
        expand(C, body.v, body.n, out, dis2, nd2, depth + 1);
        free(body.v);
    }
}

/* ---------------- #if expressions ---------------- */
typedef struct { Tok *v; int n, i; Compiler *C; int line; } PE;
static long pe_expr(PE *p, int prec);
static long pe_unary(PE *p)
{
    if (p->i >= p->n) cerr(p->C, p->line, "bad #if expression");
    Tok *t = &p->v[p->i++];
    if (t->t == TK_NUM) return (long)t->num;
    if (t->t == TK_IDENT) return 0;
    if (tok_is(t, "(")) { long v = pe_expr(p, 0); if (p->i < p->n && tok_is(&p->v[p->i], ")")) p->i++; return v; }
    if (tok_is(t, "!")) return !pe_unary(p);
    if (tok_is(t, "-")) return -pe_unary(p);
    if (tok_is(t, "+")) return pe_unary(p);
    if (tok_is(t, "~")) return ~pe_unary(p);
    cerr(p->C, p->line, "bad token '%s' in #if", t->s);
    return 0;
}
static int pe_prec(Tok *t)
{
    if (t->t != TK_OP) return -1;
    static const struct { const char *s; int p; } tab[] = {
        {"||",1},{"&&",2},{"|",3},{"^",4},{"&",5},{"==",6},{"!=",6},{"<",7},{">",7},{"<=",7},{">=",7},
        {"<<",8},{">>",8},{"+",9},{"-",9},{"*",10},{"/",10},{"%",10},{NULL,0}};
    for (int i = 0; tab[i].s; i++) if (!strcmp(t->s, tab[i].s)) return tab[i].p;
    return -1;
}
static long pe_expr(PE *p, int minprec)
{
    long l = pe_unary(p);
    for (;;) {
        if (p->i >= p->n) break;
        Tok *t = &p->v[p->i];
        int pr = pe_prec(t);
        if (pr < 0 || pr <= minprec - 1 || pr < minprec) break;
        p->i++;
        long r = pe_expr(p, pr + 1);
        const char *o = t->s;
        if (!strcmp(o, "||")) l = l || r; else if (!strcmp(o, "&&")) l = l && r;
        else if (!strcmp(o, "|")) l |= r; else if (!strcmp(o, "^")) l ^= r; else if (!strcmp(o, "&")) l &= r;
        else if (!strcmp(o, "==")) l = l == r; else if (!strcmp(o, "!=")) l = l != r;
        else if (!strcmp(o, "<")) l = l < r; else if (!strcmp(o, ">")) l = l > r;
        else if (!strcmp(o, "<=")) l = l <= r; else if (!strcmp(o, ">=")) l = l >= r;
        else if (!strcmp(o, "<<")) l <<= r; else if (!strcmp(o, ">>")) l >>= r;
        else if (!strcmp(o, "+")) l += r; else if (!strcmp(o, "-")) l -= r; else if (!strcmp(o, "*")) l *= r;
        else if (!strcmp(o, "/")) l = r ? l / r : 0; else if (!strcmp(o, "%")) l = r ? l % r : 0;
    }
    return l;
}
static long eval_if(Compiler *C, Tok *v, int n, int line)
{
    /* resolve defined() first */
    TokVec pre = {0};
    for (int i = 0; i < n; i++) {
        if (v[i].t == TK_IDENT && !strcmp(v[i].s, "defined")) {
            const char *name = NULL;
            if (i + 1 < n && tok_is(&v[i + 1], "(")) { if (i + 2 < n) name = v[i + 2].s; i += 3; }
            else if (i + 1 < n) { name = v[i + 1].s; i += 1; }
            Tok t = v[i < n ? i : n - 1]; t.t = TK_NUM; t.num = name && find_macro(C, name) ? 1 : 0; t.s = "0";
            tv_push(&pre, t);
        } else tv_push(&pre, v[i]);
    }
    TokVec ex = {0};
    expand(C, pre.v, pre.n, &ex, NULL, 0, 0);
    PE p = { ex.v, ex.n, 0, C, line };
    long r = ex.n ? pe_expr(&p, 0) : 0;
    free(pre.v); free(ex.v);
    return r;
}

/* ---------------- driver ---------------- */
int glsl_preprocess(Compiler *C, const char *src, TokVec *out)
{
    /* strip comments, join continuation lines */
    size_t len = strlen(src);
    char *buf = malloc(len + 2);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '/' && src[i + 1] == '/') { while (i < len && src[i] != '\n') i++; if (i < len) buf[o++] = '\n'; continue; }
        if (src[i] == '/' && src[i + 1] == '*') {
            i += 2; buf[o++] = ' ';
            while (i < len && !(src[i] == '*' && src[i + 1] == '/')) { if (src[i] == '\n') buf[o++] = '\n'; i++; }
            i++; continue;
        }
        if (src[i] == '\\' && (src[i + 1] == '\n' || (src[i + 1] == '\r' && src[i + 2] == '\n'))) {
            /* keep line count: emit newline later; simply join */
            i += (src[i + 1] == '\r') ? 2 : 1;
            continue;
        }
        buf[o++] = src[i] == '\r' ? ' ' : src[i];
    }
    buf[o] = 0;

    static const char *predef[] = { "GL_ES", "1", "__VERSION__", "100", "GL_FRAGMENT_PRECISION_HIGH", "1", "__LINE__", "0", NULL };
    for (int i = 0; predef[i]; i += 2) {
        struct Macro m; memset(&m, 0, sizeof m);
        m.name = predef[i];
        TokVec tv = {0}; lex_line(C, predef[i + 1], 0, &tv);
        m.body = ar_alloc(&C->ar, sizeof(Tok) * tv.n); memcpy(m.body, tv.v, sizeof(Tok) * tv.n); m.nbody = tv.n;
        free(tv.v);
        add_macro(C, m);
    }

    int cstack_active[64], cstack_taken[64], csp = 0;
    int active = 1;
    int line = 1;
    char *p = buf;
    while (*p) {
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        char *s = p;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#') {
            TokVec tv = {0};
            lex_line(C, s + 1, line, &tv);
            const char *d = tv.n ? tv.v[0].s : "";
            if (!strcmp(d, "ifdef") || !strcmp(d, "ifndef") || !strcmp(d, "if")) {
                if (csp >= 63) cerr(C, line, "#if nesting too deep");
                cstack_active[csp] = active; cstack_taken[csp] = 0; csp++;
                if (active) {
                    long v;
                    if (!strcmp(d, "if")) v = eval_if(C, tv.v + 1, tv.n - 1, line);
                    else { v = tv.n > 1 && find_macro(C, tv.v[1].s) != NULL; if (d[2] == 'n') v = !v; }
                    active = v != 0; cstack_taken[csp - 1] = active;
                } else { active = 0; cstack_taken[csp - 1] = 1; }
            } else if (!strcmp(d, "elif")) {
                if (!csp) cerr(C, line, "#elif without #if");
                if (!cstack_active[csp - 1] || cstack_taken[csp - 1]) active = 0;
                else { active = eval_if(C, tv.v + 1, tv.n - 1, line) != 0; cstack_taken[csp - 1] = active; }
            } else if (!strcmp(d, "else")) {
                if (!csp) cerr(C, line, "#else without #if");
                active = cstack_active[csp - 1] && !cstack_taken[csp - 1];
                cstack_taken[csp - 1] = 1;
            } else if (!strcmp(d, "endif")) {
                if (!csp) cerr(C, line, "#endif without #if");
                csp--; active = cstack_active[csp];
            } else if (active) {
                if (!strcmp(d, "define") && tv.n >= 2) {
                    struct Macro m; memset(&m, 0, sizeof m);
                    m.name = tv.v[1].s;
                    int bi = 2;
                    /* function-like only if '(' immediately follows name */
                    const char *np = strstr(s, m.name);
                    if (tv.n > 2 && tok_is(&tv.v[2], "(") && np && np[strlen(m.name)] == '(') {
                        m.funclike = 1; bi = 3;
                        while (bi < tv.n && !tok_is(&tv.v[bi], ")")) {
                            if (tv.v[bi].t == TK_IDENT && m.nparams < 16) m.params[m.nparams++] = tv.v[bi].s;
                            bi++;
                        }
                        bi++;
                    }
                    m.nbody = tv.n - bi > 0 ? tv.n - bi : 0;
                    m.body = ar_alloc(&C->ar, sizeof(Tok) * (m.nbody + 1));
                    if (m.nbody) memcpy(m.body, tv.v + bi, sizeof(Tok) * m.nbody);
                    add_macro(C, m);
                } else if (!strcmp(d, "undef") && tv.n >= 2) undef_macro(C, tv.v[1].s);
                else if (!strcmp(d, "error")) cerr(C, line, "#error");
                /* version, extension, pragma, line: ignored */
            }
            free(tv.v);
        } else if (active) {
            TokVec tv = {0};
            lex_line(C, p, line, &tv);
            /* gather following lines if a function-like macro call is left open */
            expand(C, tv.v, tv.n, out, NULL, 0, 0);
            free(tv.v);
        }
        line++;
        if (!e) break;
        p = e + 1;
    }
    free(buf);
    Tok eof; memset(&eof, 0, sizeof eof); eof.t = TK_EOF; eof.line = line; eof.s = "<eof>";
    tv_push(out, eof);
    return 0;
}
