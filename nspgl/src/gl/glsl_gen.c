/* GLSL AST -> flat register program, with full function inlining */
#include "glsl_int.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

int glsl_tsize_c(Compiler *C, GType t);
int glsl_type_by_name(Compiler *C, const char *s, GType *t);

#define BASE (1 << 20)   /* stack-region offsets start here until relocation */

typedef struct Sym { const char *name; GType t; int off; int readonly; int isconst; int depth; } Sym;
typedef struct Opnd {
    GType t;
    int off;
    int nsw; uint8_t sw[4];
    int lval, isconst;
    int dyn, dyn_idx, dyn_count, dyn_elem;
} Opnd;
typedef struct IVec { int *v; int n, cap; } IVec;
static void iv_push(IVec *l, int x) { if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 8; l->v = realloc(l->v, l->cap * sizeof(int)); } l->v[l->n++] = x; }

typedef struct FnCtx { int ret_off; GType ret_t; IVec rets; } FnCtx;
typedef struct LoopCtx { IVec brk, cont; } LoopCtx;

typedef struct G {
    Compiler *C;
    GOp *code; int ncode, capcode;
    float *init; int asize, acap;
    int top, maxtop;            /* stack region (relative to BASE) */
    Sym *syms; int nsyms, capsyms; int depth;
    Node **funcs; int nfuncs;
    FnCtx *fn[32]; int nfn;
    LoopCtx *loop[64]; int nloop;
    GVar *vars; int nvars, capvars;
    int inline_depth;
    struct { float v; int off; } *kp; int nk, capk;
    GShader *S;
} G;

static int tsz(G *g, GType t) { return glsl_tsize_c(g->C, t); }
static GType tf(int kind, int n) { GType t; memset(&t, 0, sizeof t); t.kind = kind; t.vec = n; return t; }
static GType elem_t(GType t) { t.arr = 0; return t; }
static int is_scalar(GType t) { return !t.arr && !t.mat && t.vec == 1 && t.kind != GK_STRUCT && t.kind != GK_VOID; }
static int is_vec(GType t) { return !t.arr && !t.mat && t.kind != GK_STRUCT && t.kind != GK_VOID && t.kind < GK_SAMPLER2D; }
static int teq(GType a, GType b) { return a.kind == b.kind && a.vec == b.vec && a.mat == b.mat && a.arr == b.arr && a.sid == b.sid; }

const char *glsl_type_name(GType t)
{
    static char bufs[4][32]; static int bi;
    char *buf = bufs[bi = (bi + 1) & 3];
    const char *b = "?";
    if (t.kind == GK_VOID) b = "void";
    else if (t.kind == GK_SAMPLER2D) b = "sampler2D";
    else if (t.kind == GK_SAMPLERCUBE) b = "samplerCube";
    else if (t.kind == GK_STRUCT) b = "struct";
    else if (t.mat) { snprintf(buf, 32, "mat%d", t.mat); b = buf; }
    else if (t.vec == 1) b = t.kind == GK_FLOAT ? "float" : t.kind == GK_INT ? "int" : "bool";
    else { snprintf(buf, 32, "%svec%d", t.kind == GK_FLOAT ? "" : t.kind == GK_INT ? "i" : "b", t.vec); b = buf; }
    if (t.arr) { char tmp[32]; snprintf(tmp, sizeof tmp, "%s", b); snprintf(buf, 32, "%s[%d]", tmp, t.arr); b = buf; }
    return b;
}

int glsl_type_size(GType t)
{
    int e = t.mat ? t.mat * t.vec : (t.kind == GK_VOID ? 0 : t.vec);
    return e * (t.arr > 0 ? t.arr : 1);
}

/* ---------- emission ---------- */
static int emit(G *g, int op, int d, int a, int b, int c, int n, int fl)
{
    if (g->ncode == g->capcode) { g->capcode = g->capcode ? g->capcode * 2 : 256; g->code = realloc(g->code, g->capcode * sizeof(GOp)); }
    GOp *o = &g->code[g->ncode];
    o->op = op; o->d = d; o->a = a; o->b = b; o->c = c; o->n = n; o->fl = fl; o->pad = 0;
    return g->ncode++;
}
static int here(G *g) { return g->ncode; }
static void patch(G *g, int at, int target)
{
    GOp *o = &g->code[at];
    if (o->op == OP_JMP) o->a = target; else o->b = target;
}

static int tmp(G *g, int n)
{
    int o = g->top;
    g->top += n > 0 ? n : 1;
    if (g->top > g->maxtop) g->maxtop = g->top;
    return BASE + o;
}
static int aalloc(G *g, int n)
{
    if (n < 1) n = 1;
    if (g->asize + n > g->acap) {
        g->acap = (g->asize + n) * 2;
        g->init = realloc(g->init, g->acap * sizeof(float));
    }
    int o = g->asize;
    memset(g->init + o, 0, n * sizeof(float));
    g->asize += n;
    return o;
}
static int konst(G *g, float v)
{
    for (int i = 0; i < g->nk; i++) if (!memcmp(&g->kp[i].v, &v, sizeof v)) return g->kp[i].off;
    int o = aalloc(g, 1);
    g->init[o] = v;
    if (g->nk == g->capk) { g->capk = g->capk ? g->capk * 2 : 32; g->kp = realloc(g->kp, g->capk * sizeof(*g->kp)); }
    g->kp[g->nk].v = v; g->kp[g->nk].off = o; g->nk++;
    return o;
}
static int konst_vec(G *g, const float *v, int n)
{
    if (n == 1) return konst(g, v[0]);
    /* search existing A region for identical run (cheap dedup) */
    int o = aalloc(g, n);
    memcpy(g->init + o, v, n * sizeof(float));
    return o;
}
static float cval(G *g, int off) { return g->init[off]; }

static Opnd mkop(GType t, int off) { Opnd o; memset(&o, 0, sizeof o); o.t = t; o.off = off; return o; }
static Opnd mkconst(G *g, GType t, const float *v)
{
    Opnd o = mkop(t, konst_vec(g, v, tsz(g, t)));
    o.isconst = 1;
    return o;
}

/* ---------- symbols ---------- */
static void push_sym(G *g, const char *name, GType t, int off, int ro, int isconst)
{
    if (g->nsyms == g->capsyms) { g->capsyms = g->capsyms ? g->capsyms * 2 : 64; g->syms = realloc(g->syms, g->capsyms * sizeof(Sym)); }
    Sym *s = &g->syms[g->nsyms++];
    s->name = name; s->t = t; s->off = off; s->readonly = ro; s->isconst = isconst; s->depth = g->depth;
}
static Sym *find_sym(G *g, const char *name)
{
    for (int i = g->nsyms - 1; i >= 0; i--) if (!strcmp(g->syms[i].name, name)) return &g->syms[i];
    return NULL;
}
static void scope_push(G *g) { g->depth++; }
static void scope_pop(G *g) { while (g->nsyms && g->syms[g->nsyms - 1].depth >= g->depth) g->nsyms--; g->depth--; }

static void add_var(G *g, const char *name, GType t, int off, int storage)
{
    if (g->nvars == g->capvars) { g->capvars = g->capvars ? g->capvars * 2 : 16; g->vars = realloc(g->vars, g->capvars * sizeof(GVar)); }
    GVar *v = &g->vars[g->nvars++];
    memset(v, 0, sizeof *v);
    snprintf(v->name, sizeof v->name, "%s", name);
    v->type = t; v->off = off; v->storage = storage;
}
/* export a uniform, flattening structs */
static void export_var(G *g, const char *name, GType t, int off, int storage)
{
    if (t.kind != GK_STRUCT) { add_var(g, name, t, off, storage); return; }
    SDef *sd = &g->C->structs[t.sid];
    int cnt = t.arr ? t.arr : 1;
    for (int e = 0; e < cnt; e++) {
        for (int f = 0; f < sd->nf; f++) {
            char nm[64];
            if (t.arr) snprintf(nm, sizeof nm, "%s[%d].%s", name, e, sd->f[f].name);
            else snprintf(nm, sizeof nm, "%s.%s", name, sd->f[f].name);
            export_var(g, ar_strdup(&g->C->ar, nm, -1), sd->f[f].t, off + e * sd->size + sd->f[f].off, storage);
        }
    }
}

/* ---------- operand access ---------- */
static uint32_t packsw(const uint8_t *sw, int n) { uint32_t c = 0; for (int i = 0; i < n; i++) c |= (uint32_t)sw[i] << (8 * i); return c; }

static int rv(G *g, Opnd *o)
{
    int base = o->off;
    if (o->dyn) {
        base = tmp(g, o->dyn_elem);
        emit(g, OP_IDXLD, base, o->off, o->dyn_idx, o->dyn_count, o->dyn_elem, 0);
    }
    if (!o->nsw) return base;
    int contig = 1;
    for (int i = 1; i < o->nsw; i++) if (o->sw[i] != o->sw[0] + i) contig = 0;
    if (contig) return base + o->sw[0];
    int t = tmp(g, o->nsw);
    emit(g, OP_SWZ, t, base, -1, (int)packsw(o->sw, o->nsw), o->nsw, 0);
    return t;
}
static Opnd mat(G *g, Opnd o)   /* materialize to plain readable operand */
{
    if (!o.dyn && !o.nsw) { o.lval = 0; return o; }
    int isc = o.isconst && !o.dyn;
    Opnd r = mkop(o.t, rv(g, &o));
    r.isconst = isc && r.off < BASE;
    return r;
}
static void store(G *g, Opnd *lv, int src, int srcscalar, int line)
{
    int n = tsz(g, lv->t);
    if (!lv->lval) cerr(g->C, line, "l-value required");
    int fl = srcscalar && n > 1 ? FL_SA : 0;
    if (fl) { int t = tmp(g, n); emit(g, OP_MOV, t, src, -1, -1, n, FL_SA); src = t; }
    if (lv->dyn) {
        if (lv->nsw) {
            int t = tmp(g, lv->dyn_elem);
            emit(g, OP_IDXLD, t, lv->off, lv->dyn_idx, lv->dyn_count, lv->dyn_elem, 0);
            emit(g, OP_SCAT, t, src, -1, (int)packsw(lv->sw, lv->nsw), lv->nsw, 0);
            emit(g, OP_IDXST, lv->off, t, lv->dyn_idx, lv->dyn_count, lv->dyn_elem, 0);
        } else emit(g, OP_IDXST, lv->off, src, lv->dyn_idx, lv->dyn_count, lv->dyn_elem, 0);
    } else if (lv->nsw) emit(g, OP_SCAT, lv->off, src, -1, (int)packsw(lv->sw, lv->nsw), lv->nsw, 0);
    else emit(g, OP_MOV, lv->off, src, -1, -1, n, 0);
}

/* ---------- expressions ---------- */
static Opnd ex(G *g, Node *n);
static void stmt(G *g, Node *n);
static Opnd binop(G *g, int op, Opnd A, Opnd B, int line);

enum { OPC_EQ = 256, OPC_NE, OPC_LE, OPC_GE, OPC_AND, OPC_OR, OPC_XOR, OPC_SHL, OPC_SHR };

static int fold_ok(Opnd *a, Opnd *b) { return a->isconst && !a->nsw && !a->dyn && b->isconst && !b->nsw && !b->dyn; }

static Opnd binop(G *g, int op, Opnd A, Opnd B, int line)
{
    Compiler *C = g->C;
    if (A.t.arr || B.t.arr || A.t.kind == GK_STRUCT || B.t.kind == GK_STRUCT) {
        if (op == OPC_EQ || op == OPC_NE) {
            int n = tsz(g, A.t);
            if (n != tsz(g, B.t)) cerr(C, line, "type mismatch in comparison");
            int a = rv(g, &A), b = rv(g, &B), d = tmp(g, 1);
            emit(g, op == OPC_EQ ? OP_EQ : OP_NE, d, a, b, -1, n, 0);
            return mkop(tf(GK_BOOL, 1), d);
        }
        cerr(C, line, "invalid operands to binary operator");
    }
    int na = tsz(g, A.t), nb = tsz(g, B.t);
    switch (op) {
    case '+': case '-': case '*': case '/': case '%': {
        if (A.t.kind == GK_BOOL || B.t.kind == GK_BOOL) cerr(C, line, "arithmetic on bool");
        if (op == '*' && A.t.mat && B.t.mat) {
            if (A.t.mat != B.t.mat) cerr(C, line, "matrix size mismatch");
            int a = rv(g, &A), b = rv(g, &B), d = tmp(g, na);
            emit(g, OP_MATMAT, d, a, b, -1, A.t.mat, 0);
            return mkop(A.t, d);
        }
        if (op == '*' && A.t.mat && !B.t.mat && B.t.vec > 1) {
            if (B.t.vec != A.t.mat) cerr(C, line, "matrix/vector size mismatch");
            int a = rv(g, &A), b = rv(g, &B), d = tmp(g, B.t.vec);
            emit(g, OP_MATVEC, d, a, b, -1, A.t.mat, 0);
            return mkop(B.t, d);
        }
        if (op == '*' && B.t.mat && !A.t.mat && A.t.vec > 1) {
            if (A.t.vec != B.t.mat) cerr(C, line, "vector/matrix size mismatch");
            int a = rv(g, &A), b = rv(g, &B), d = tmp(g, A.t.vec);
            emit(g, OP_VECMAT, d, a, b, -1, B.t.mat, 0);
            return mkop(A.t, d);
        }
        int fl = 0, n;
        GType rt;
        if (na == nb) { n = na; rt = A.t.mat ? A.t : (B.t.mat ? B.t : A.t); }
        else if (na == 1) { fl = FL_SA; n = nb; rt = B.t; }
        else if (nb == 1) { fl = FL_SB; n = na; rt = A.t; }
        else cerr(C, line, "operand size mismatch (%s vs %s)", glsl_type_name(A.t), glsl_type_name(B.t));
        if (A.t.kind == GK_FLOAT || B.t.kind == GK_FLOAT) rt.kind = GK_FLOAT;
        int isint = rt.kind == GK_INT;
        if (fold_ok(&A, &B)) {
            float r[16];
            for (int i = 0; i < n; i++) {
                float x = cval(g, A.off + (fl & FL_SA ? 0 : i)), y = cval(g, B.off + (fl & FL_SB ? 0 : i));
                switch (op) { case '+': r[i] = x + y; break; case '-': r[i] = x - y; break; case '*': r[i] = x * y; break;
                              case '/': r[i] = y != 0 ? x / y : 0; if (isint) r[i] = (float)(int)r[i]; break;
                              default: r[i] = y != 0 ? x - y * floorf(x / y) : 0; }
            }
            return mkconst(g, rt, r);
        }
        int a = rv(g, &A), b = rv(g, &B), d = tmp(g, n);
        int o = op == '+' ? OP_ADD : op == '-' ? OP_SUB : op == '*' ? OP_MUL : op == '/' ? OP_DIV : OP_MOD;
        emit(g, o, d, a, b, -1, n, fl);
        if (isint && op == '/') emit(g, OP_UFN, d, d, -1, UF_TRUNC, n, 0);
        return mkop(rt, d);
    }
    case '<': case '>': case OPC_LE: case OPC_GE: {
        if (na != 1 || nb != 1) cerr(C, line, "relational operator requires scalars");
        int a = rv(g, &A), b = rv(g, &B), d = tmp(g, 1);
        emit(g, op == '<' ? OP_LT : op == '>' ? OP_GT : op == OPC_LE ? OP_LE : OP_GE, d, a, b, -1, 1, 0);
        return mkop(tf(GK_BOOL, 1), d);
    }
    case OPC_EQ: case OPC_NE: {
        if (na != nb) cerr(C, line, "type mismatch in comparison");
        int a = rv(g, &A), b = rv(g, &B), d = tmp(g, 1);
        emit(g, op == OPC_EQ ? OP_EQ : OP_NE, d, a, b, -1, na, 0);
        return mkop(tf(GK_BOOL, 1), d);
    }
    case OPC_XOR: {
        int a = rv(g, &A), b = rv(g, &B), d = tmp(g, 1);
        emit(g, OP_XOR, d, a, b, -1, 1, 0);
        return mkop(tf(GK_BOOL, 1), d);
    }
    }
    cerr(C, line, "unsupported operator");
    return A;
}

static Opnd one_const(G *g, GType t)
{
    float v[16]; int n = tsz(g, t);
    for (int i = 0; i < n; i++) v[i] = 1;
    return mkconst(g, t, v);
}

/* ---- swizzle parsing ---- */
static int swz_parse(const char *s, uint8_t *out)
{
    static const char *sets[3] = { "xyzw", "rgba", "stpq" };
    int n = strlen(s);
    if (n < 1 || n > 4) return 0;
    for (int set = 0; set < 3; set++) {
        int ok = 1;
        for (int i = 0; i < n && ok; i++) {
            const char *p = strchr(sets[set], s[i]);
            if (!p) ok = 0; else out[i] = p - sets[set];
        }
        if (ok) return n;
    }
    return 0;
}

/* ---- constructors ---- */
static Opnd construct(G *g, GType T, Opnd *args, int nargs, int line)
{
    Compiler *C = g->C;
    int n = tsz(g, T);
    if (nargs == 0) cerr(C, line, "constructor needs arguments");
    if (T.kind == GK_STRUCT || T.arr) {
        int d = tmp(g, n), pos = 0;
        for (int i = 0; i < nargs; i++) {
            int an = tsz(g, args[i].t);
            int a = rv(g, &args[i]);
            if (pos + an > n) cerr(C, line, "too many constructor arguments");
            emit(g, OP_MOV, d + pos, a, -1, -1, an, 0);
            pos += an;
        }
        return mkop(T, d);
    }
    int allconst = 1, needconv = 0;
    for (int i = 0; i < nargs; i++) {
        if (!args[i].isconst || args[i].dyn) allconst = 0;
        if (args[i].t.kind != T.kind) needconv = 1;
        if (args[i].t.kind == GK_STRUCT || args[i].t.arr) cerr(C, line, "bad constructor argument");
    }
    int convop = T.kind == GK_INT ? UF_TRUNC : T.kind == GK_BOOL ? UF_TOBOOL : -1;
    if (convop < 0) needconv = 0;
    /* matrix from scalar / matrix */
    if (T.mat && nargs == 1 && is_scalar(args[0].t)) {
        if (allconst) {
            float v[16] = {0}, s = cval(g, args[0].off + (args[0].nsw ? args[0].sw[0] : 0));
            for (int i = 0; i < T.mat; i++) v[i * T.mat + i] = s;
            return mkconst(g, T, v);
        }
        int a = rv(g, &args[0]), d = tmp(g, n);
        emit(g, OP_MATDIAG, d, a, -1, -1, T.mat, 0);
        return mkop(T, d);
    }
    if (T.mat && nargs == 1 && args[0].t.mat) {
        int a = rv(g, &args[0]), d = tmp(g, n);
        emit(g, OP_MATRESIZE, d, a, -1, args[0].t.mat, T.mat, 0);
        return mkop(T, d);
    }
    if (nargs == 1 && is_scalar(args[0].t) && n > 1) {
        /* broadcast */
        if (allconst) {
            float v[16], s = cval(g, args[0].off + (args[0].nsw ? args[0].sw[0] : 0));
            if (convop == UF_TRUNC) s = (float)(int)s; else if (convop == UF_TOBOOL && needconv) s = s != 0;
            for (int i = 0; i < n; i++) v[i] = s;
            return mkconst(g, T, v);
        }
        int a = rv(g, &args[0]), d = tmp(g, n);
        emit(g, OP_MOV, d, a, -1, -1, n, FL_SA);
        if (needconv) emit(g, OP_UFN, d, d, -1, convop, n, 0);
        return mkop(T, d);
    }
    if (allconst) {
        float v[16]; int pos = 0;
        for (int i = 0; i < nargs && pos < n; i++) {
            int an = tsz(g, args[i].t);
            for (int k = 0; k < an && pos < n; k++) {
                int off = args[i].nsw ? args[i].off + args[i].sw[k] : args[i].off + k;
                float s = cval(g, off);
                if (needconv) s = convop == UF_TRUNC ? (float)(int)s : (s != 0);
                v[pos++] = s;
            }
        }
        if (pos < n) cerr(C, line, "not enough data for constructor");
        return mkconst(g, T, v);
    }
    int d = tmp(g, n), pos = 0;
    for (int i = 0; i < nargs && pos < n; i++) {
        int an = tsz(g, args[i].t);
        int take = an < n - pos ? an : n - pos;
        int a = rv(g, &args[i]);
        emit(g, OP_MOV, d + pos, a, -1, -1, take, 0);
        pos += take;
    }
    if (pos < n) cerr(C, line, "not enough data for constructor");
    if (needconv) emit(g, OP_UFN, d, d, -1, convop, n, 0);
    return mkop(T, d);
}

/* ---- builtin functions ---- */
static int gen_ok(GType t) { return t.kind == GK_FLOAT && !t.mat && !t.arr; }

static Opnd builtin(G *g, const char *nm, Opnd *A, int na, int line, int *found)
{
    Compiler *C = g->C;
    *found = 1;
    static const struct { const char *n; int f; } unary[] = {
        {"sin",UF_SIN},{"cos",UF_COS},{"tan",UF_TAN},{"asin",UF_ASIN},{"acos",UF_ACOS},{"exp",UF_EXP},
        {"log",UF_LOG},{"exp2",UF_EXP2},{"log2",UF_LOG2},{"sqrt",UF_SQRT},{"inversesqrt",UF_RSQRT},
        {"abs",UF_ABS},{"sign",UF_SIGN},{"floor",UF_FLOOR},{"ceil",UF_CEIL},{"fract",UF_FRACT},
        {"radians",UF_RAD},{"degrees",UF_DEG},{"not",UF_NOT},{"trunc",UF_TRUNC},{NULL,0}};
    for (int i = 0; unary[i].n; i++) if (!strcmp(nm, unary[i].n)) {
        if (na != 1) cerr(C, line, "%s expects 1 argument", nm);
        int n = tsz(g, A[0].t);
        if (A[0].isconst && !A[0].dyn && unary[i].f != UF_NOT) {
            /* fold */
            float v[16];
            for (int k = 0; k < n; k++) {
                float x = cval(g, A[0].off + (A[0].nsw ? A[0].sw[k] : k)), r = x;
                switch (unary[i].f) {
                case UF_SIN: r = sinf(x); break; case UF_COS: r = cosf(x); break; case UF_TAN: r = tanf(x); break;
                case UF_ASIN: r = asinf(x); break; case UF_ACOS: r = acosf(x); break; case UF_EXP: r = expf(x); break;
                case UF_LOG: r = logf(x); break; case UF_EXP2: r = exp2f(x); break; case UF_LOG2: r = log2f(x); break;
                case UF_SQRT: r = sqrtf(x); break; case UF_RSQRT: r = 1.0f / sqrtf(x); break; case UF_ABS: r = fabsf(x); break;
                case UF_SIGN: r = (x > 0) - (x < 0); break; case UF_FLOOR: r = floorf(x); break; case UF_CEIL: r = ceilf(x); break;
                case UF_FRACT: r = x - floorf(x); break; case UF_RAD: r = x * 0.017453292f; break; case UF_DEG: r = x * 57.29578f; break;
                case UF_TRUNC: r = (float)(int)x; break; }
                v[k] = r;
            }
            return mkconst(g, A[0].t, v);
        }
        int a = rv(g, &A[0]), d = tmp(g, n);
        emit(g, OP_UFN, d, a, -1, unary[i].f, n, 0);
        return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "atan")) {
        int n = tsz(g, A[0].t);
        if (na == 1) { int a = rv(g, &A[0]), d = tmp(g, n); emit(g, OP_UFN, d, a, -1, UF_ATAN, n, 0); return mkop(A[0].t, d); }
        int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, n);
        emit(g, OP_ATAN2, d, a, b, -1, n, tsz(g, A[1].t) == 1 && n > 1 ? FL_SB : 0);
        return mkop(A[0].t, d);
    }
    static const struct { const char *n; int op; } bin[] = {
        {"pow",OP_POW},{"mod",OP_MOD},{"min",OP_MIN},{"max",OP_MAX},{"matrixCompMult",OP_MUL},{NULL,0}};
    for (int i = 0; bin[i].n; i++) if (!strcmp(nm, bin[i].n)) {
        if (na != 2) cerr(C, line, "%s expects 2 arguments", nm);
        int n = tsz(g, A[0].t), nb = tsz(g, A[1].t);
        int fl = (nb == 1 && n > 1) ? FL_SB : 0;
        if (!fl && nb != n) cerr(C, line, "%s: argument size mismatch", nm);
        int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, n);
        emit(g, bin[i].op, d, a, b, -1, n, fl);
        return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "step")) {
        if (na != 2) cerr(C, line, "step expects 2 arguments");
        int n = tsz(g, A[1].t), ne = tsz(g, A[0].t);
        int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, n);
        emit(g, OP_STEP, d, a, b, -1, n, (ne == 1 && n > 1) ? FL_SA : 0);
        return mkop(A[1].t, d);
    }
    if (!strcmp(nm, "clamp")) {
        if (na != 3) cerr(C, line, "clamp expects 3 arguments");
        int n = tsz(g, A[0].t);
        int fl = (tsz(g, A[1].t) == 1 && n > 1 ? FL_SB : 0) | (tsz(g, A[2].t) == 1 && n > 1 ? FL_SC : 0);
        int a = rv(g, &A[0]), b = rv(g, &A[1]), c = rv(g, &A[2]), d = tmp(g, n);
        emit(g, OP_CLAMP, d, a, b, c, n, fl);
        return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "mix")) {
        if (na != 3) cerr(C, line, "mix expects 3 arguments");
        int n = tsz(g, A[0].t);
        int fl = (tsz(g, A[2].t) == 1 && n > 1) ? FL_SC : 0;
        int a = rv(g, &A[0]), b = rv(g, &A[1]), c = rv(g, &A[2]), d = tmp(g, n);
        if (A[2].t.kind == GK_BOOL) {   /* mix(x, y, bvec) -> select */
            emit(g, OP_MIX, d, a, b, c, n, fl);
        } else emit(g, OP_MIX, d, a, b, c, n, fl);
        return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "smoothstep")) {
        if (na != 3) cerr(C, line, "smoothstep expects 3 arguments");
        int n = tsz(g, A[2].t);
        int fl = (tsz(g, A[0].t) == 1 && n > 1 ? FL_SA : 0) | (tsz(g, A[1].t) == 1 && n > 1 ? FL_SB : 0);
        int a = rv(g, &A[0]), b = rv(g, &A[1]), c = rv(g, &A[2]), d = tmp(g, n);
        emit(g, OP_SSTEP, d, a, b, c, n, fl);
        return mkop(A[2].t, d);
    }
    if (!strcmp(nm, "length")) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), d = tmp(g, 1);
        emit(g, OP_LEN, d, a, -1, -1, n, 0); return mkop(tf(GK_FLOAT, 1), d);
    }
    if (!strcmp(nm, "distance") || !strcmp(nm, "dot")) {
        if (na != 2) cerr(C, line, "%s expects 2 arguments", nm);
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, 1);
        emit(g, nm[0] == 'd' && nm[1] == 'i' ? OP_DIST : OP_DOT, d, a, b, -1, n, 0); return mkop(tf(GK_FLOAT, 1), d);
    }
    if (!strcmp(nm, "cross")) {
        int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, 3);
        emit(g, OP_CROSS, d, a, b, -1, 3, 0); return mkop(tf(GK_FLOAT, 3), d);
    }
    if (!strcmp(nm, "normalize")) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), d = tmp(g, n);
        emit(g, OP_NORM, d, a, -1, -1, n, 0); return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "reflect")) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, n);
        emit(g, OP_REFLECT, d, a, b, -1, n, 0); return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "refract")) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), b = rv(g, &A[1]), c = rv(g, &A[2]), d = tmp(g, n);
        emit(g, OP_REFRACT, d, a, b, c, n, 0); return mkop(A[0].t, d);
    }
    if (!strcmp(nm, "faceforward")) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), b = rv(g, &A[1]), c = rv(g, &A[2]), d = tmp(g, n);
        emit(g, OP_FFWD, d, a, b, c, n, 0); return mkop(A[0].t, d);
    }
    static const struct { const char *n; int op; } rel[] = {
        {"lessThan",OP_LT},{"lessThanEqual",OP_LE},{"greaterThan",OP_GT},{"greaterThanEqual",OP_GE},
        {"equal",OP_EQC},{"notEqual",OP_NEC},{NULL,0}};
    for (int i = 0; rel[i].n; i++) if (!strcmp(nm, rel[i].n)) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), b = rv(g, &A[1]), d = tmp(g, n);
        emit(g, rel[i].op, d, a, b, -1, n, 0); return mkop(tf(GK_BOOL, n), d);
    }
    if (!strcmp(nm, "any") || !strcmp(nm, "all")) {
        int n = tsz(g, A[0].t); int a = rv(g, &A[0]), d = tmp(g, 1);
        emit(g, nm[1] == 'n' ? OP_ANY : OP_ALL, d, a, -1, -1, n, 0); return mkop(tf(GK_BOOL, 1), d);
    }
    if (!strncmp(nm, "texture", 7)) {
        const char *s = nm + 7;
        int cube = !strncmp(s, "Cube", 4);
        int proj = strstr(s, "Proj") != NULL;
        if (na < 2) cerr(C, line, "%s needs 2 arguments", nm);
        int smp = rv(g, &A[0]), crd = rv(g, &A[1]);
        int bias = -1;
        if (na >= 3 && !strstr(s, "Grad")) bias = rv(g, &A[2]);
        if (strstr(s, "Lod")) bias = -1;  /* no mip levels */
        int d = tmp(g, 4);
        if (cube) emit(g, OP_TEXCUBE, d, smp, crd, bias, 3, 0);
        else if (proj) emit(g, OP_TEXPROJ, d, smp, crd, bias, tsz(g, A[1].t), 0);
        else emit(g, OP_TEX, d, smp, crd, bias, 2, 0);
        return mkop(tf(GK_FLOAT, 4), d);
    }
    if (!strcmp(nm, "dFdx") || !strcmp(nm, "dFdy") || !strcmp(nm, "fwidth")) {
        float z[16] = {0};
        return mkconst(g, A[0].t, z);
    }
    *found = 0;
    return A[0];
}

/* ---- user function inlining ---- */
static int param_match(G *g, Node *f, Opnd *A, int na, int strict)
{
    if (f->list.n != na) return 0;
    for (int i = 0; i < na; i++) {
        GType p = f->list.v[i]->ts.t, a = A[i].t;
        if (strict ? !teq(p, a) : tsz(g, p) != tsz(g, a)) return 0;
    }
    return 1;
}
static const char *base_name(Node *n)
{
    while (n && (n->k == N_FIELD || n->k == N_INDEX)) n = n->a;
    return n && n->k == N_IDENT ? n->name : NULL;
}
/* does the subtree possibly write variable `name`? (conservative) */
static int writes_name(G *g, Node *n, const char *name)
{
    if (!n) return 0;
    if (n->k == N_ASSIGN || n->k == N_PREINC || n->k == N_PREDEC || n->k == N_POSTINC || n->k == N_POSTDEC) {
        const char *b = base_name(n->a);
        if (!b || !strcmp(b, name)) return 1;
    }
    if (n->k == N_CALL) {
        for (int i = 0; i < g->nfuncs; i++) if (!strcmp(g->funcs[i]->name, n->name)) {
            Node *f = g->funcs[i];
            for (int k = 0; k < f->list.n && k < n->list.n; k++)
                if (f->list.v[k]->qual & Q_OUT) { const char *b = base_name(n->list.v[k]); if (!b || !strcmp(b, name)) return 1; }
        }
    }
    if (writes_name(g, n->a, name) || writes_name(g, n->b, name) || writes_name(g, n->c, name) || writes_name(g, n->d, name)) return 1;
    if (writes_name(g, n->init, name) || writes_name(g, n->body, name)) return 1;
    for (int i = 0; i < n->list.n; i++) if (writes_name(g, n->list.v[i], name)) return 1;
    return 0;
}

static Opnd call_user(G *g, Node *call, Opnd *A, int na, int *found)
{
    Compiler *C = g->C;
    Node *f = NULL;
    for (int pass = 0; pass < 2 && !f; pass++)
        for (int i = 0; i < g->nfuncs; i++) {
            Node *x = g->funcs[i];
            if (!strcmp(x->name, call->name) && x->body && param_match(g, x, A, na, pass == 0)) { f = x; break; }
        }
    if (!f) { *found = 0; return mkop(tf(GK_VOID, 0), 0); }
    *found = 1;
    if (g->inline_depth > 24 || g->nfn >= 31) cerr(C, call->line, "function nesting too deep (recursion?)");
    /* bind parameters */
    int poff[32];
    if (na > 32) cerr(C, call->line, "too many arguments");
    for (int i = 0; i < na; i++) {
        Node *p = f->list.v[i];
        int sz = tsz(g, p->ts.t);
        if (!(p->qual & Q_OUT) && p->name && *p->name && !writes_name(g, f->body, p->name)) {
            /* read-only parameter: alias the argument instead of copying */
            int a = rv(g, &A[i]);
            int lv = A[i].lval && !A[i].dyn && !A[i].nsw;
            if (!(lv && a < BASE)) { poff[i] = a; continue; }
            poff[i] = tmp(g, sz);
            emit(g, OP_MOV, poff[i], a, -1, -1, sz, 0);
            continue;
        }
        poff[i] = tmp(g, sz);
        if ((p->qual & Q_OUT) != Q_OUT || (p->qual & Q_INOUT) == Q_INOUT) {
            if (!(p->qual == Q_OUT)) { int a = rv(g, &A[i]); emit(g, OP_MOV, poff[i], a, -1, -1, sz, 0); }
        }
    }
    FnCtx fc; memset(&fc, 0, sizeof fc);
    fc.ret_t = f->ts.t;
    if (f->ts.t.kind != GK_VOID) fc.ret_off = tmp(g, tsz(g, f->ts.t));
    int saved_nsyms = g->nsyms;
    /* hide caller locals: functions only see globals + params */
    scope_push(g);
    Sym *saved = NULL; int nsaved = 0;
    {
        /* temporarily shadow non-global symbols by renaming depth: simplest is to copy globals view */
        nsaved = g->nsyms;
        saved = malloc(sizeof(Sym) * (nsaved + 1));
        memcpy(saved, g->syms, sizeof(Sym) * nsaved);
        int w = 0;
        for (int i = 0; i < g->nsyms; i++) if (g->syms[i].depth == 0) g->syms[w++] = g->syms[i];
        g->nsyms = w;
    }
    for (int i = 0; i < na; i++) {
        Node *p = f->list.v[i];
        if (p->name && *p->name) push_sym(g, p->name, p->ts.t, poff[i], 0, 0);
    }
    g->fn[g->nfn++] = &fc;
    g->inline_depth++;
    int savedtop = g->top;
    stmt(g, f->body);
    g->top = savedtop;
    g->inline_depth--;
    g->nfn--;
    int end = here(g);
    for (int i = 0; i < fc.rets.n; i++) patch(g, fc.rets.v[i], end);
    free(fc.rets.v);
    /* restore symbols */
    memcpy(g->syms, saved, sizeof(Sym) * nsaved);
    g->nsyms = nsaved;
    free(saved);
    g->depth--;
    (void)saved_nsyms;
    /* copy back out params */
    for (int i = 0; i < na; i++) {
        Node *p = f->list.v[i];
        if (p->qual & Q_OUT) store(g, &A[i], poff[i], 0, call->line);
    }
    if (f->ts.t.kind == GK_VOID) return mkop(f->ts.t, 0);
    return mkop(f->ts.t, fc.ret_off);
}

static Opnd ex(G *g, Node *n)
{
    Compiler *C = g->C;
    switch (n->k) {
    case N_NUM: {
        float v = (float)n->num;
        GType t = tf(n->isbool ? GK_BOOL : n->isfloat ? GK_FLOAT : GK_INT, 1);
        Opnd o = mkop(t, konst(g, v)); o.isconst = 1;
        return o;
    }
    case N_IDENT: {
        Sym *s = find_sym(g, n->name);
        if (!s) cerr(C, n->line, "'%s' : undeclared identifier", n->name);
        Opnd o = mkop(s->t, s->off);
        o.lval = !s->readonly; o.isconst = s->isconst;
        return o;
    }
    case N_COMMA: {
        ex(g, n->a);
        return ex(g, n->b);
    }
    case N_BIN: {
        if (n->op == OPC_AND || n->op == OPC_OR) {
            Opnd A = ex(g, n->a);
            int r = tmp(g, 1);
            int a = rv(g, &A);
            emit(g, OP_MOV, r, a, -1, -1, 1, 0);
            int j = emit(g, n->op == OPC_AND ? OP_JZ : OP_JNZ, -1, r, -1, -1, 0, 0);
            int savedtop = g->top;
            Opnd B = ex(g, n->b);
            int b = rv(g, &B);
            emit(g, OP_MOV, r, b, -1, -1, 1, 0);
            g->top = savedtop;
            patch(g, j, here(g));
            return mkop(tf(GK_BOOL, 1), r);
        }
        Opnd A = ex(g, n->a);
        Opnd B = ex(g, n->b);
        return binop(g, n->op, A, B, n->line);
    }
    case N_UN: {
        Opnd A = ex(g, n->a);
        int sz = tsz(g, A.t);
        if (n->op == '-') {
            if (A.isconst && !A.dyn) {
                float v[16];
                for (int i = 0; i < sz; i++) v[i] = -cval(g, A.off + (A.nsw ? A.sw[i] : i));
                return mkconst(g, A.t, v);
            }
            int a = rv(g, &A), d = tmp(g, sz);
            emit(g, OP_NEG, d, a, -1, -1, sz, 0);
            return mkop(A.t, d);
        }
        if (n->op == '!') {
            int a = rv(g, &A), d = tmp(g, 1);
            emit(g, OP_UFN, d, a, -1, UF_NOT, 1, 0);
            return mkop(tf(GK_BOOL, 1), d);
        }
        cerr(C, n->line, "unsupported unary operator");
        break;
    }
    case N_ASSIGN: {
        Opnd L = ex(g, n->a);
        if (!L.lval) cerr(C, n->line, "l-value required");
        Opnd R = ex(g, n->b);
        int src;
        if (n->op != '=') {
            Opnd Lr = L; Lr.lval = 0;
            Opnd res = binop(g, n->op, Lr, R, n->line);
            src = rv(g, &res);
            store(g, &L, src, 0, n->line);
        } else {
            int nl = tsz(g, L.t), nr = tsz(g, R.t);
            if (nl != nr && nr != 1) cerr(C, n->line, "cannot convert from '%s' to '%s'", glsl_type_name(R.t), glsl_type_name(L.t));
            src = rv(g, &R);
            store(g, &L, src, nr == 1 && nl > 1, n->line);
        }
        Opnd o = L; o.lval = 1;
        return o;
    }
    case N_PREINC: case N_PREDEC: case N_POSTINC: case N_POSTDEC: {
        Opnd L = ex(g, n->a);
        if (!L.lval) cerr(C, n->line, "l-value required");
        int sz = tsz(g, L.t);
        int old = tmp(g, sz);
        int lv = rv(g, &L);
        emit(g, OP_MOV, old, lv, -1, -1, sz, 0);
        int one = konst(g, 1.0f), d = tmp(g, sz);
        emit(g, (n->k == N_PREINC || n->k == N_POSTINC) ? OP_ADD : OP_SUB, d, old, one, -1, sz, FL_SB);
        store(g, &L, d, 0, n->line);
        return mkop(L.t, (n->k == N_POSTINC || n->k == N_POSTDEC) ? old : d);
    }
    case N_COND: {
        Opnd c = ex(g, n->a);
        int cv = rv(g, &c);
        int j1 = emit(g, OP_JZ, -1, cv, -1, -1, 0, 0);
        int savedtop = g->top;
        Opnd B = ex(g, n->b);
        int sz = tsz(g, B.t);
        g->top = savedtop;
        /* result slot allocated outside both branches */
        int r = tmp(g, sz);
        savedtop = g->top;
        int b = rv(g, &B);
        emit(g, OP_MOV, r, b, -1, -1, sz, 0);
        int j2 = emit(g, OP_JMP, -1, -1, -1, -1, 0, 0);
        patch(g, j1, here(g));
        Opnd Cc = ex(g, n->c);
        int cc = rv(g, &Cc);
        emit(g, OP_MOV, r, cc, -1, -1, sz, tsz(g, Cc.t) == 1 && sz > 1 ? FL_SA : 0);
        patch(g, j2, here(g));
        g->top = savedtop;
        return mkop(B.t, r);
    }
    case N_CTOR: {
        Opnd args[16];
        if (n->list.n > 16) cerr(C, n->line, "too many constructor arguments");
        for (int i = 0; i < n->list.n; i++) args[i] = ex(g, n->list.v[i]);
        return construct(g, n->ts.t, args, n->list.n, n->line);
    }
    case N_CALL: {
        Opnd args[32];
        if (n->list.n > 32) cerr(C, n->line, "too many arguments");
        for (int i = 0; i < n->list.n; i++) args[i] = ex(g, n->list.v[i]);
        int found = 0;
        Opnd r = call_user(g, n, args, n->list.n, &found);
        if (found) return r;
        /* struct constructor by name */
        GType st;
        if (glsl_type_by_name(C, n->name, &st)) return construct(g, st, args, n->list.n, n->line);
        if (n->list.n == 0) cerr(C, n->line, "no matching function '%s'", n->name);
        r = builtin(g, n->name, args, n->list.n, n->line, &found);
        if (found) return r;
        cerr(C, n->line, "'%s' : no matching overloaded function found", n->name);
        break;
    }
    case N_FIELD: {
        Opnd B = ex(g, n->a);
        if (n->op == 'L') {    /* .length() */
            float v = (float)(B.t.arr ? B.t.arr : B.t.vec);
            Opnd o = mkop(tf(GK_INT, 1), konst(g, v)); o.isconst = 1; return o;
        }
        if (B.t.kind == GK_STRUCT && !B.t.arr) {
            SDef *sd = &C->structs[B.t.sid];
            for (int i = 0; i < sd->nf; i++) if (!strcmp(sd->f[i].name, n->name)) {
                if (B.dyn || B.nsw) { int lv = B.lval && 0; B = mat(g, B); B.lval = lv; }
                Opnd o = B; o.t = sd->f[i].t; o.off = B.off + sd->f[i].off;
                return o;
            }
            cerr(C, n->line, "no field '%s' in struct", n->name);
        }
        if (B.t.mat || B.t.arr || B.t.kind == GK_STRUCT || B.t.kind >= GK_SAMPLER2D)
            cerr(C, n->line, "invalid field selection '%s'", n->name);
        uint8_t sw[4];
        int ns = swz_parse(n->name, sw);
        if (!ns) cerr(C, n->line, "invalid swizzle '%s'", n->name);
        for (int i = 0; i < ns; i++) if (sw[i] >= B.t.vec) cerr(C, n->line, "swizzle '%s' out of range", n->name);
        Opnd o = B;
        if (B.nsw) { for (int i = 0; i < ns; i++) o.sw[i] = B.sw[sw[i]]; }
        else memcpy(o.sw, sw, 4);
        o.nsw = ns;
        o.t = tf(B.t.kind, ns);
        /* repeated components => not an l-value */
        for (int i = 0; i < ns; i++) for (int j = i + 1; j < ns; j++) if (o.sw[i] == o.sw[j]) o.lval = 0;
        return o;
    }
    case N_INDEX: {
        Opnd B = ex(g, n->a);
        Opnd I = ex(g, n->b);
        GType et; int esz, cnt;
        if (B.t.arr) { et = elem_t(B.t); esz = tsz(g, et); cnt = B.t.arr; }
        else if (B.t.mat) { et = tf(GK_FLOAT, B.t.vec); esz = B.t.vec; cnt = B.t.mat; }
        else if (is_vec(B.t)) { et = tf(B.t.kind, 1); esz = 1; cnt = B.t.vec; }
        else { cerr(C, n->line, "'[' : not an array, matrix or vector"); return B; }
        if (B.dyn || B.nsw) { int lv = B.lval; B = mat(g, B); B.lval = lv && 0; }
        if (I.isconst && !I.dyn) {
            int k = (int)cval(g, I.off + (I.nsw ? I.sw[0] : 0));
            if (k < 0 || k >= cnt) cerr(C, n->line, "index %d out of range", k);
            Opnd o = B; o.t = et; o.off = B.off + k * esz; o.isconst = B.isconst;
            return o;
        }
        Opnd o = B; o.t = et;
        o.dyn = 1; o.dyn_idx = rv(g, &I); o.dyn_count = cnt; o.dyn_elem = esz;
        o.isconst = 0;
        return o;
    }
    }
    cerr(C, n->line, "unsupported expression");
    return mkop(tf(GK_VOID, 0), 0);
}

/* ---------- statements ---------- */
static void decl_local(G *g, Node *d)
{
    for (int i = 0; i < d->list.n; i++) {
        Node *v = d->list.v[i];
        GType t = v->ts.t;
        if (t.kind == GK_VOID) cerr(g->C, v->line, "void variable");
        if ((v->qual & Q_CONST) && v->init) {
            int saved = g->top;
            Opnd I = ex(g, v->init);
            if (I.isconst && !I.dyn) {
                Opnd m = mat(g, I);
                if (m.isconst) { push_sym(g, v->name, t, m.off, 1, 1); g->top = saved; continue; }
            }
            g->top = saved;
        }
        int off = tmp(g, tsz(g, t));
        if (v->init) {
            int saved = g->top;
            Opnd I = ex(g, v->init);
            int nr = tsz(g, I.t), nl = tsz(g, t);
            if (nr != nl && nr != 1) cerr(g->C, v->line, "initializer type mismatch: '%s' to '%s'", glsl_type_name(I.t), glsl_type_name(t));
            int src = rv(g, &I);
            emit(g, OP_MOV, off, src, -1, -1, nl, nr == 1 && nl > 1 ? FL_SA : 0);
            g->top = saved;
        }
        push_sym(g, v->name, t, off, (v->qual & Q_CONST) != 0, 0);
    }
}

static void stmt(G *g, Node *n)
{
    Compiler *C = g->C;
    int saved = g->top;
    switch (n->k) {
    case S_EMPTY: break;
    case S_BLOCK:
        scope_push(g);
        for (int i = 0; i < n->list.n; i++) stmt(g, n->list.v[i]);
        scope_pop(g);
        g->top = saved;
        return;
    case S_DECL: decl_local(g, n); return;   /* keep slots */
    case S_EXPR: ex(g, n->a); break;
    case S_IF: {
        Opnd c = ex(g, n->a);
        int cv = rv(g, &c);
        int j1 = emit(g, OP_JZ, -1, cv, -1, -1, 0, 0);
        g->top = saved;
        stmt(g, n->b);
        g->top = saved;
        if (n->c) {
            int j2 = emit(g, OP_JMP, -1, -1, -1, -1, 0, 0);
            patch(g, j1, here(g));
            stmt(g, n->c);
            patch(g, j2, here(g));
        } else patch(g, j1, here(g));
        break;
    }
    case S_FOR: case S_WHILE: case S_DO: {
        scope_push(g);
        LoopCtx lc; memset(&lc, 0, sizeof lc);
        if (g->nloop >= 63) cerr(C, n->line, "loops nested too deeply");
        if (n->k == S_FOR && n->a) stmt(g, n->a);
        int after_init = g->top;
        int top = here(g), jexit = -1;
        Node *cond = n->k == S_FOR ? n->b : n->a;
        Node *body = n->k == S_FOR ? n->d : n->b;
        if (n->k != S_DO && cond) {
            Opnd c = ex(g, cond);
            int cv = rv(g, &c);
            jexit = emit(g, OP_JZ, -1, cv, -1, -1, 0, 0);
            g->top = after_init;
        }
        g->loop[g->nloop++] = &lc;
        stmt(g, body);
        g->nloop--;
        g->top = after_init;
        int contpos = here(g);
        for (int i = 0; i < lc.cont.n; i++) patch(g, lc.cont.v[i], contpos);
        if (n->k == S_FOR && n->c) { ex(g, n->c); g->top = after_init; }
        if (n->k == S_DO) {
            Opnd c = ex(g, cond);
            int cv = rv(g, &c);
            emit(g, OP_JNZ, -1, cv, top, -1, 0, 0);
        } else emit(g, OP_JMP, -1, top, -1, -1, 0, 0);
        int end = here(g);
        if (jexit >= 0) patch(g, jexit, end);
        for (int i = 0; i < lc.brk.n; i++) patch(g, lc.brk.v[i], end);
        free(lc.brk.v); free(lc.cont.v);
        scope_pop(g);
        break;
    }
    case S_BREAK: case S_CONTINUE: {
        if (!g->nloop) cerr(C, n->line, "break/continue outside loop");
        LoopCtx *lc = g->loop[g->nloop - 1];
        int j = emit(g, OP_JMP, -1, -1, -1, -1, 0, 0);
        iv_push(n->k == S_BREAK ? &lc->brk : &lc->cont, j);
        break;
    }
    case S_RETURN: {
        FnCtx *fc = g->fn[g->nfn - 1];
        if (n->a) {
            Opnd r = ex(g, n->a);
            if (fc->ret_t.kind == GK_VOID) cerr(C, n->line, "void function cannot return a value");
            int src = rv(g, &r);
            int nr = tsz(g, r.t), nl = tsz(g, fc->ret_t);
            if (nr != nl && nr != 1) cerr(C, n->line, "return type mismatch");
            emit(g, OP_MOV, fc->ret_off, src, -1, -1, nl, nr == 1 && nl > 1 ? FL_SA : 0);
        }
        int j = emit(g, OP_JMP, -1, -1, -1, -1, 0, 0);
        iv_push(&fc->rets, j);
        break;
    }
    case S_DISCARD:
        if (g->C->stage != 1) cerr(C, n->line, "discard only allowed in fragment shaders");
        emit(g, OP_KILL, -1, -1, -1, -1, 0, 0);
        g->S->has_discard = 1;
        break;
    default:
        cerr(C, n->line, "unsupported statement");
    }
    g->top = saved;
}

/* ---------- globals ---------- */
static int builtin_var(G *g, const char *name, GType t, int storage)
{
    int off = aalloc(g, tsz(g, t));
    push_sym(g, name, t, off, 0, 0);
    add_var(g, name, t, off, storage);
    return off;
}

static void decl_global(G *g, Node *d)
{
    Compiler *C = g->C;
    for (int i = 0; i < d->list.n; i++) {
        Node *v = d->list.v[i];
        GType t = v->ts.t;
        int q = v->qual;
        if (find_sym(g, v->name) && find_sym(g, v->name)->depth == 0 && !(q & (Q_UNIFORM | Q_VARYING)))
            cerr(C, v->line, "'%s' : redefinition", v->name);
        if (find_sym(g, v->name) && (q & (Q_UNIFORM | Q_VARYING))) continue;
        if (q & Q_ATTRIBUTE) {
            if (C->stage != 0) cerr(C, v->line, "attribute in fragment shader");
            int off = aalloc(g, tsz(g, t));
            push_sym(g, v->name, t, off, 1, 0);
            add_var(g, v->name, t, off, GS_ATTRIBUTE);
        } else if (q & Q_UNIFORM) {
            int off = aalloc(g, tsz(g, t));
            push_sym(g, v->name, t, off, 1, 0);
            export_var(g, v->name, t, off, GS_UNIFORM);
        } else if (q & Q_VARYING) {
            int off = aalloc(g, tsz(g, t));
            push_sym(g, v->name, t, off, C->stage == 1, 0);
            add_var(g, v->name, t, off, GS_VARYING);
        } else {
            if ((q & Q_CONST) && v->init) {
                int saved = g->top;
                Opnd I = ex(g, v->init);
                if (I.isconst && !I.dyn) {
                    Opnd m = mat(g, I);
                    if (m.isconst) { push_sym(g, v->name, t, m.off, 1, 1); g->top = saved; continue; }
                }
                g->top = saved;
            }
            int off = aalloc(g, tsz(g, t));
            if (v->init) {
                int saved = g->top;
                Opnd I = ex(g, v->init);
                int nr = tsz(g, I.t), nl = tsz(g, t);
                if (nr != nl && nr != 1) cerr(C, v->line, "initializer type mismatch");
                if (I.isconst && !I.dyn && !I.nsw && nr == nl) memcpy(g->init + off, g->init + I.off, nl * sizeof(float));
                else { int src = rv(g, &I); emit(g, OP_MOV, off, src, -1, -1, nl, nr == 1 && nl > 1 ? FL_SA : 0); }
                g->top = saved;
            }
            push_sym(g, v->name, t, off, (q & Q_CONST) != 0, 0);
        }
    }
}

/* which fields of an op are memory addresses: bit0=d bit1=a bit2=b bit3=c */
static int addr_mask(int op)
{
    switch (op) {
    case OP_MOV: case OP_SWZ: case OP_SCAT: case OP_NEG: case OP_UFN: case OP_LEN: case OP_NORM:
    case OP_ANY: case OP_ALL: case OP_MATDIAG: case OP_MATRESIZE: return 3;
    case OP_CLAMP: case OP_MIX: case OP_SSTEP: case OP_REFRACT: case OP_FFWD: return 15;
    case OP_JMP: case OP_KILL: case OP_END: case OP_NOP: return 0;
    case OP_JZ: case OP_JNZ: return 2;
    case OP_TEX: case OP_TEXPROJ: case OP_TEXCUBE: return 15;
    case OP_IDXLD: case OP_IDXST: return 7;
    default: return 7;
    }
}


/* ---------- peephole: fold "op T <- ...; MOV X <- T" into "op X <- ..." ---------- */
static int rd_width(const GOp *o, int which)   /* which: 1=a 2=b 3=c 0=d(read) */
{
    int n = o->n;
    switch (o->op) {
    case OP_MOV: return which == 1 ? ((o->fl & FL_SA) ? 1 : n) : 0;
    case OP_SWZ: return which == 1 ? 4 : 0;
    case OP_SCAT: return which == 1 ? n : which == 0 ? 4 : 0;
    case OP_MATVEC: return which == 1 ? n * n : which == 2 ? n : 0;
    case OP_VECMAT: return which == 1 ? n : which == 2 ? n * n : 0;
    case OP_MATMAT: return (which == 1 || which == 2) ? n * n : 0;
    case OP_TEX: case OP_TEXPROJ: case OP_TEXCUBE: return which == 1 ? 1 : which == 2 ? 4 : which == 3 ? 1 : 0;
    case OP_IDXLD: return which == 1 ? n * o->c : which == 2 ? 1 : 0;
    case OP_IDXST: return which == 1 ? n : which == 2 ? 1 : which == 0 ? n * o->c : 0;
    case OP_MATDIAG: return which == 1 ? 1 : 0;
    case OP_MATRESIZE: return which == 1 ? o->c * o->c : 0;
    case OP_JZ: case OP_JNZ: return which == 1 ? 1 : 0;
    case OP_REFRACT: return which == 3 ? 1 : (which ? n : 0);
    case OP_CROSS: return which == 1 || which == 2 ? 3 : 0;
    }
    if (which == 0) return 0;
    if (which == 1 && (o->fl & FL_SA)) return 1;
    if (which == 2 && (o->fl & FL_SB)) return 1;
    if (which == 3 && (o->fl & FL_SC)) return 1;
    return n ? n : 1;
}
static int reads_range(const GOp *o, int lo, int hi)
{
    int m = addr_mask(o->op);
    int vals[4] = { o->d, o->a, o->b, o->c };
    for (int w = 0; w < 4; w++) {
        if (w > 0 && !(m & (1 << w))) continue;
        int len = rd_width(o, w), v = vals[w];
        if (len && v >= 0 && v < hi && v + len > lo) return 1;
    }
    return 0;
}
static int is_simple_dst_op(int op)
{
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_NEG: case OP_MOD: case OP_MIN: case OP_MAX:
    case OP_POW: case OP_ATAN2: case OP_STEP: case OP_CLAMP: case OP_MIX: case OP_SSTEP: case OP_UFN:
    case OP_DOT: case OP_LEN: case OP_DIST: case OP_NORM: case OP_LT: case OP_LE: case OP_GT: case OP_GE:
    case OP_EQ: case OP_NE: case OP_AND: case OP_OR: case OP_XOR: case OP_MOV: case OP_SWZ:
    case OP_MATVEC: case OP_VECMAT: case OP_MATMAT: case OP_CROSS: case OP_TEX: case OP_TEXPROJ: case OP_TEXCUBE:
        return 1;
    }
    return 0;
}
static int op_width(const GOp *o)
{
    switch (o->op) {
    case OP_DOT: case OP_LEN: case OP_DIST: case OP_EQ: case OP_NE: return 1;
    case OP_TEX: case OP_TEXPROJ: case OP_TEXCUBE: return 4;
    case OP_MATMAT: return o->n * o->n;
    case OP_CROSS: return 3;
    }
    return o->n;
}
static void peephole(GOp *code, int n, int stack_base)
{
    int changed = 1;
    while (changed) {
        changed = 0;
        /* jump targets block folding across labels */
        char *target = calloc(n + 1, 1);
        for (int i = 0; i < n; i++) {
            if (code[i].op == OP_JMP && code[i].a >= 0 && code[i].a <= n) target[code[i].a] = 1;
            if ((code[i].op == OP_JZ || code[i].op == OP_JNZ) && code[i].b >= 0 && code[i].b <= n) target[code[i].b] = 1;
        }
        for (int i = 0; i + 1 < n; i++) if (code[i].op == OP_JMP && code[i].a == i + 1) { code[i].op = OP_NOP; changed = 1; }
        if (changed) { /* recompute targets */
            memset(target, 0, n + 1);
            for (int i = 0; i < n; i++) {
                if (code[i].op == OP_JMP && code[i].a >= 0 && code[i].a <= n) target[code[i].a] = 1;
                if ((code[i].op == OP_JZ || code[i].op == OP_JNZ) && code[i].b >= 0 && code[i].b <= n) target[code[i].b] = 1;
            }
        }
        for (int i = 0; i + 1 < n; i++) {
            GOp *p = &code[i], *mv = &code[i + 1];
            if (mv->op != OP_MOV || mv->fl || target[i + 1]) continue;
            if (!is_simple_dst_op(p->op) || p->d != mv->a || p->d < stack_base) continue;
            int w = op_width(p);
            if (w != mv->n) continue;
            int T = p->d, X = mv->d;
            /* X must not overlap p's inputs (aliasing) */
            if (reads_range(p, X, X + w)) continue;
            /* T must be dead after the MOV: scan forward in straight-line code */
            int dead = 0;
            for (int k = i + 2; k < n; k++) {
                const GOp *o = &code[k];
                if (target[k] || o->op == OP_JMP || o->op == OP_JZ || o->op == OP_JNZ) break;
                if (reads_range(o, T, T + w)) break;
                if (o->op == OP_END || o->op == OP_KILL) { dead = 1; break; }
                int om = addr_mask(o->op);
                if ((om & 1) && is_simple_dst_op(o->op) && o->d <= T && o->d + op_width(o) >= T + w) { dead = 1; break; }
            }
            if (!dead) continue;
            p->d = X;
            mv->op = OP_NOP;
            changed = 1;
        }
        /* compact NOPs, remapping jump targets */
        int *map = malloc(sizeof(int) * (n + 1)), w = 0;
        for (int i = 0; i < n; i++) { map[i] = w; if (code[i].op != OP_NOP) w++; }
        map[n] = w;
        for (int i = 0; i < n; i++) {
            if (code[i].op == OP_JMP) code[i].a = map[code[i].a];
            if (code[i].op == OP_JZ || code[i].op == OP_JNZ) code[i].b = map[code[i].b];
        }
        int o2 = 0;
        for (int i = 0; i < n; i++) if (code[i].op != OP_NOP) code[o2++] = code[i];
        n = o2;
        free(map); free(target);
    }
    (void)0;
}

GShader *glsl_compile(const char *src, int stage)
{
    GShader *S = calloc(1, sizeof(GShader));
    Compiler *C = calloc(1, sizeof(Compiler));
    G *g = calloc(1, sizeof(G));
    S->stage = stage;
    C->stage = stage;
    g->C = C; g->S = S;
    S->off_position = S->off_pointsize = S->off_fragcolor = S->off_fragcoord = S->off_frontfacing = S->off_pointcoord = -1;
    if (!setjmp(C->jb)) {
        TokVec toks = {0};
        glsl_preprocess(C, src, &toks);
        Node *root = glsl_parse(C, &toks);
        /* builtins */
        if (stage == 0) {
            S->off_position = builtin_var(g, "gl_Position", tf(GK_FLOAT, 4), GS_BUILTIN);
            S->off_pointsize = builtin_var(g, "gl_PointSize", tf(GK_FLOAT, 1), GS_BUILTIN);
            g->init[S->off_pointsize] = 1.0f;
        } else {
            S->off_fragcoord = builtin_var(g, "gl_FragCoord", tf(GK_FLOAT, 4), GS_BUILTIN);
            S->off_frontfacing = builtin_var(g, "gl_FrontFacing", tf(GK_BOOL, 1), GS_BUILTIN);
            S->off_pointcoord = builtin_var(g, "gl_PointCoord", tf(GK_FLOAT, 2), GS_BUILTIN);
            S->off_fragcolor = builtin_var(g, "gl_FragColor", tf(GK_FLOAT, 4), GS_BUILTIN);
            GType fd = tf(GK_FLOAT, 4); fd.arr = 1;
            push_sym(g, "gl_FragData", fd, S->off_fragcolor, 0, 0);
        }
        static const struct { const char *n; int v; } lim[] = {
            {"gl_MaxVertexAttribs",8},{"gl_MaxVertexUniformVectors",128},{"gl_MaxVaryingVectors",8},
            {"gl_MaxVertexTextureImageUnits",4},{"gl_MaxCombinedTextureImageUnits",8},{"gl_MaxTextureImageUnits",8},
            {"gl_MaxFragmentUniformVectors",64},{"gl_MaxDrawBuffers",1},{NULL,0}};
        for (int i = 0; lim[i].n; i++) push_sym(g, lim[i].n, tf(GK_INT, 1), konst(g, (float)lim[i].v), 1, 1);
        /* gl_DepthRange */
        {
            float dr[3] = { 0, 1, 1 };
            GType t = tf(GK_FLOAT, 3);
            push_sym(g, "__gl_DepthRange", t, konst_vec(g, dr, 3), 1, 1);
        }
        FnCtx mainctx; memset(&mainctx, 0, sizeof mainctx);
        mainctx.ret_t = tf(GK_VOID, 0);
        g->fn[g->nfn++] = &mainctx;
        Node *mainf = NULL;
        for (int i = 0; i < root->list.n; i++) {
            Node *x = root->list.v[i];
            if (x->k == D_FUNC) {
                g->funcs = realloc(g->funcs, (g->nfuncs + 1) * sizeof(Node *));
                g->funcs[g->nfuncs++] = x;
                if (!strcmp(x->name, "main") && x->body) mainf = x;
            } else if (x->k == S_DECL) decl_global(g, x);
        }
        if (!mainf) cerr(C, 0, "missing main()");
        g->inline_depth = 1;
        stmt(g, mainf->body);
        int end = emit(g, OP_END, -1, -1, -1, -1, 0, 0);
        for (int i = 0; i < mainctx.rets.n; i++) patch(g, mainctx.rets.v[i], end);
        free(mainctx.rets.v);
        free(toks.v);
        /* relocate stack region after constants/globals */
        int A = g->asize;
        for (int i = 0; i < g->ncode; i++) {
            GOp *o = &g->code[i];
            int m = addr_mask(o->op);
            if ((m & 1) && o->d >= BASE) o->d = o->d - BASE + A;
            if ((m & 2) && o->a >= BASE) o->a = o->a - BASE + A;
            if ((m & 4) && o->b >= BASE) o->b = o->b - BASE + A;
            if ((m & 8) && o->c >= BASE) o->c = o->c - BASE + A;
        }
        peephole(g->code, g->ncode, A);
        { int k = 0; while (g->code[k].op != OP_END) k++; g->ncode = k + 1; }
        S->memsize = A + g->maxtop + 4;
        S->init = calloc(S->memsize, sizeof(float));
        memcpy(S->init, g->init, A * sizeof(float));
        S->code = g->code; S->ncode = g->ncode;
        S->vars = g->vars; S->nvars = g->nvars;
        for (int i = 0; i < S->ncode; i++) if (S->code[i].op == OP_IDXLD || S->code[i].op == OP_IDXST) {
            /* fields: d,a,b addresses; c=count; n=elem */
        }
        /* detect gl_PointSize writes */
        if (stage == 0) for (int i = 0; i < S->ncode; i++) {
            GOp *o = &S->code[i];
            if (addr_mask(o->op) & 1 && o->d >= 0 && o->d == S->off_pointsize) S->writes_pointsize = 1;
        }
        S->ok = 1;
    } else {
        free(g->code);
        free(g->vars);
        S->ok = 0;
    }
    S->log = C->log ? C->log : strdup("");
    free(g->init); free(g->syms); free(g->funcs); free(g->kp);
    free(C->macros);
    ar_free(&C->ar);
    free(g); free(C);
    return S;
}

void glsl_free(GShader *s)
{
    if (!s) return;
    free(s->code); free(s->init); free(s->vars); free(s->log);
    free(s);
}
