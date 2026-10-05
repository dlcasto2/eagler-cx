/* GLSL ES 1.00 parser -> AST */
#include "glsl_int.h"
#include <stdlib.h>
#include <string.h>

typedef struct P { Compiler *C; Tok *t; int i, n; } P;

void nl_push(NList *l, Node *n)
{
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 4; l->v = realloc(l->v, l->cap * sizeof(Node *)); }
    l->v[l->n++] = n;
}

static Tok *cur(P *p) { return &p->t[p->i]; }
static Tok *peekn(P *p, int k) { int j = p->i + k; return &p->t[j < p->n ? j : p->n - 1]; }
static int is_op(Tok *t, const char *s) { return t->t == TK_OP && !strcmp(t->s, s); }
static int is_id(Tok *t, const char *s) { return t->t == TK_IDENT && !strcmp(t->s, s); }
static int accept(P *p, const char *s) { if (is_op(cur(p), s)) { p->i++; return 1; } return 0; }
static void expect(P *p, const char *s)
{
    if (!accept(p, s)) cerr(p->C, cur(p)->line, "expected '%s' but found '%s'", s, cur(p)->s ? cur(p)->s : "?");
}
static Node *mk(P *p, int k)
{
    Node *n = ar_alloc(&p->C->ar, sizeof(Node));
    n->k = k; n->line = cur(p)->line;
    return n;
}

static int is_precision_word(const char *s)
{
    return !strcmp(s, "lowp") || !strcmp(s, "mediump") || !strcmp(s, "highp");
}

/* returns 1 and fills type if token is a type name */
static int type_of_name(Compiler *C, const char *s, GType *t)
{
    static const struct { const char *n; uint8_t k, v, m; } tab[] = {
        {"void",GK_VOID,0,0},{"float",GK_FLOAT,1,0},{"vec2",GK_FLOAT,2,0},{"vec3",GK_FLOAT,3,0},{"vec4",GK_FLOAT,4,0},
        {"int",GK_INT,1,0},{"ivec2",GK_INT,2,0},{"ivec3",GK_INT,3,0},{"ivec4",GK_INT,4,0},
        {"bool",GK_BOOL,1,0},{"bvec2",GK_BOOL,2,0},{"bvec3",GK_BOOL,3,0},{"bvec4",GK_BOOL,4,0},
        {"mat2",GK_FLOAT,2,2},{"mat3",GK_FLOAT,3,3},{"mat4",GK_FLOAT,4,4},
        {"sampler2D",GK_SAMPLER2D,1,0},{"samplerCube",GK_SAMPLERCUBE,1,0},{NULL,0,0,0}};
    for (int i = 0; tab[i].n; i++)
        if (!strcmp(tab[i].n, s)) { memset(t, 0, sizeof *t); t->kind = tab[i].k; t->vec = tab[i].v; t->mat = tab[i].m; return 1; }
    for (int i = 0; i < C->nstructs; i++)
        if (!strcmp(C->structs[i].name, s)) { memset(t, 0, sizeof *t); t->kind = GK_STRUCT; t->sid = i; t->vec = 1; return 1; }
    return 0;
}

static int struct_size(Compiler *C, GType t);
static int tsize(Compiler *C, GType t)
{
    int e;
    if (t.kind == GK_STRUCT) e = C->structs[t.sid].size;
    else if (t.mat) e = t.mat * t.vec;
    else if (t.kind == GK_VOID) e = 0;
    else e = t.vec;
    return e * (t.arr > 0 ? t.arr : 1);
}
static int struct_size(Compiler *C, GType t) { return tsize(C, t); }

static Node *expr(P *p);
static Node *assign_expr(P *p);
static int const_int(P *p, Node *n);

static int starts_type(P *p, Tok *t)
{
    GType g;
    if (t->t != TK_IDENT) return 0;
    if (is_precision_word(t->s) || !strcmp(t->s, "struct")) return 1;
    return type_of_name(p->C, t->s, &g);
}

static void parse_struct(P *p, GType *out);

/* parse [precision] type */
static GType parse_type(P *p)
{
    while (cur(p)->t == TK_IDENT && is_precision_word(cur(p)->s)) p->i++;
    GType t;
    if (is_id(cur(p), "struct")) { parse_struct(p, &t); return t; }
    if (cur(p)->t != TK_IDENT || !type_of_name(p->C, cur(p)->s, &t))
        cerr(p->C, cur(p)->line, "expected type, found '%s'", cur(p)->s);
    p->i++;
    return t;
}

static void parse_struct(P *p, GType *out)
{
    Compiler *C = p->C;
    p->i++; /* struct */
    const char *name = "<anon>";
    if (cur(p)->t == TK_IDENT) { name = cur(p)->s; p->i++; }
    if (C->nstructs >= 16) cerr(C, cur(p)->line, "too many structs");
    SDef *sd = &C->structs[C->nstructs];
    memset(sd, 0, sizeof *sd);
    sd->name = name;
    expect(p, "{");
    int off = 0;
    while (!accept(p, "}")) {
        GType ft = parse_type(p);
        do {
            if (cur(p)->t != TK_IDENT) cerr(C, cur(p)->line, "expected field name");
            const char *fn = cur(p)->s; p->i++;
            GType t = ft;
            if (accept(p, "[")) { t.arr = const_int(p, expr(p)); expect(p, "]"); }
            if (sd->nf >= 32) cerr(C, cur(p)->line, "too many struct fields");
            sd->f[sd->nf].name = fn; sd->f[sd->nf].t = t; sd->f[sd->nf].off = off;
            off += tsize(C, t);
            sd->nf++;
        } while (accept(p, ","));
        expect(p, ";");
    }
    sd->size = off;
    memset(out, 0, sizeof *out);
    out->kind = GK_STRUCT; out->sid = C->nstructs; out->vec = 1;
    C->nstructs++;
}

/* ------------ constant int evaluation for array sizes ------------ */
static Node **g_consts; static int g_nconsts;  /* const int decls seen (global) */
static int const_int(P *p, Node *n)
{
    switch (n->k) {
    case N_NUM: return (int)n->num;
    case N_UN: { int v = const_int(p, n->a); return n->op == '-' ? -v : v; }
    case N_BIN: {
        int a = const_int(p, n->a), b = const_int(p, n->b);
        switch (n->op) { case '+': return a + b; case '-': return a - b; case '*': return a * b;
                         case '/': return b ? a / b : 0; case '%': return b ? a % b : 0; }
        break; }
    case N_IDENT:
        for (int i = g_nconsts - 1; i >= 0; i--)
            if (!strcmp(g_consts[i]->name, n->name) && g_consts[i]->init) return const_int(p, g_consts[i]->init);
        break;
    case N_CTOR: if (n->list.n == 1) return const_int(p, n->list.v[0]); break;
    }
    cerr(p->C, n->line, "constant integer expression required");
    return 0;
}

/* ------------ expressions ------------ */
static Node *primary(P *p)
{
    Tok *t = cur(p);
    Node *n;
    if (t->t == TK_NUM) {
        n = mk(p, N_NUM); n->num = t->num; n->isfloat = t->isfloat; p->i++; return n;
    }
    if (t->t == TK_IDENT) {
        if (!strcmp(t->s, "true") || !strcmp(t->s, "false")) {
            n = mk(p, N_NUM); n->num = t->s[0] == 't'; n->isbool = 1; p->i++; return n;
        }
        GType ty;
        if (type_of_name(p->C, t->s, &ty) && is_op(peekn(p, 1), "(")) {
            n = mk(p, N_CTOR); n->ts.t = ty; p->i += 2;
            if (!is_op(cur(p), ")")) do nl_push(&n->list, assign_expr(p)); while (accept(p, ","));
            expect(p, ")");
            return n;
        }
        if (type_of_name(p->C, t->s, &ty) && is_op(peekn(p, 1), "[")) {
            /* array constructor float[3](...) */
            n = mk(p, N_CTOR); n->ts.t = ty; p->i += 2;
            if (!is_op(cur(p), "]")) n->ts.t.arr = const_int(p, expr(p));
            expect(p, "]"); expect(p, "(");
            if (!is_op(cur(p), ")")) do nl_push(&n->list, assign_expr(p)); while (accept(p, ","));
            expect(p, ")");
            if (!n->ts.t.arr) n->ts.t.arr = n->list.n;
            return n;
        }
        if (is_op(peekn(p, 1), "(")) {
            n = mk(p, N_CALL); n->name = t->s; p->i += 2;
            if (is_id(cur(p), "void") && is_op(peekn(p, 1), ")")) p->i++;
            if (!is_op(cur(p), ")")) do nl_push(&n->list, assign_expr(p)); while (accept(p, ","));
            expect(p, ")");
            return n;
        }
        n = mk(p, N_IDENT); n->name = t->s; p->i++;
        return n;
    }
    if (accept(p, "(")) { n = expr(p); expect(p, ")"); return n; }
    cerr(p->C, t->line, "unexpected '%s'", t->s);
    return NULL;
}

static Node *postfix(P *p)
{
    Node *n = primary(p);
    for (;;) {
        if (accept(p, "[")) { Node *x = mk(p, N_INDEX); x->a = n; x->b = expr(p); expect(p, "]"); n = x; }
        else if (accept(p, ".")) {
            Node *x = mk(p, N_FIELD); x->a = n;
            if (cur(p)->t != TK_IDENT) cerr(p->C, cur(p)->line, "expected field name");
            x->name = cur(p)->s; p->i++;
            if (is_op(cur(p), "(")) {   /* .length() */
                p->i++; expect(p, ")"); x->op = 'L';
            }
            n = x;
        }
        else if (accept(p, "++")) { Node *x = mk(p, N_POSTINC); x->a = n; n = x; }
        else if (accept(p, "--")) { Node *x = mk(p, N_POSTDEC); x->a = n; n = x; }
        else break;
    }
    return n;
}

static Node *unary(P *p)
{
    Tok *t = cur(p);
    if (t->t == TK_OP) {
        if (accept(p, "++")) { Node *n = mk(p, N_PREINC); n->a = unary(p); return n; }
        if (accept(p, "--")) { Node *n = mk(p, N_PREDEC); n->a = unary(p); return n; }
        if (is_op(t, "-") || is_op(t, "+") || is_op(t, "!") || is_op(t, "~")) {
            p->i++;
            Node *n = mk(p, N_UN); n->op = t->s[0]; n->a = unary(p);
            if (n->op == '+') return n->a;
            if (n->op == '-' && n->a->k == N_NUM && !n->a->isbool) { n->a->num = -n->a->num; return n->a; }
            return n;
        }
    }
    return postfix(p);
}

/* binary op ids: single char or codes */
enum { OPC_EQ = 256, OPC_NE, OPC_LE, OPC_GE, OPC_AND, OPC_OR, OPC_XOR, OPC_SHL, OPC_SHR };
static int binprec(Tok *t, int *op)
{
    if (t->t != TK_OP) return -1;
    const char *s = t->s;
    static const struct { const char *s; int p, op; } tab[] = {
        {"||",1,OPC_OR},{"^^",2,OPC_XOR},{"&&",3,OPC_AND},{"|",4,'|'},{"^",5,'^'},{"&",6,'&'},
        {"==",7,OPC_EQ},{"!=",7,OPC_NE},{"<",8,'<'},{">",8,'>'},{"<=",8,OPC_LE},{">=",8,OPC_GE},
        {"<<",9,OPC_SHL},{">>",9,OPC_SHR},{"+",10,'+'},{"-",10,'-'},{"*",11,'*'},{"/",11,'/'},{"%",11,'%'},{NULL,0,0}};
    for (int i = 0; tab[i].s; i++) if (!strcmp(s, tab[i].s)) { *op = tab[i].op; return tab[i].p; }
    return -1;
}
static Node *binary(P *p, int minprec)
{
    Node *l = unary(p);
    for (;;) {
        int op, pr = binprec(cur(p), &op);
        if (pr < 0 || pr < minprec) break;
        p->i++;
        Node *r = binary(p, pr + 1);
        Node *n = mk(p, N_BIN); n->op = op; n->a = l; n->b = r; l = n;
    }
    return l;
}
static Node *cond_expr(P *p)
{
    Node *c = binary(p, 1);
    if (accept(p, "?")) {
        Node *n = mk(p, N_COND); n->a = c; n->b = expr(p); expect(p, ":"); n->c = assign_expr(p);
        return n;
    }
    return c;
}
static Node *assign_expr(P *p)
{
    Node *l = cond_expr(p);
    Tok *t = cur(p);
    if (t->t == TK_OP) {
        int op = 0;
        if (!strcmp(t->s, "=")) op = '=';
        else if (!strcmp(t->s, "+=")) op = '+'; else if (!strcmp(t->s, "-=")) op = '-';
        else if (!strcmp(t->s, "*=")) op = '*'; else if (!strcmp(t->s, "/=")) op = '/';
        else if (!strcmp(t->s, "%=")) op = '%';
        if (op) {
            p->i++;
            Node *n = mk(p, N_ASSIGN); n->op = op; n->a = l; n->b = assign_expr(p);
            return n;
        }
    }
    return l;
}
static Node *expr(P *p)
{
    Node *l = assign_expr(p);
    while (accept(p, ",")) { Node *n = mk(p, N_COMMA); n->a = l; n->b = assign_expr(p); l = n; }
    return l;
}

/* ------------ statements ------------ */
static Node *statement(P *p);

static int parse_qualifiers(P *p)
{
    int q = 0;
    for (;;) {
        Tok *t = cur(p);
        if (t->t != TK_IDENT) break;
        if (!strcmp(t->s, "const")) q |= Q_CONST;
        else if (!strcmp(t->s, "attribute")) q |= Q_ATTRIBUTE;
        else if (!strcmp(t->s, "uniform")) q |= Q_UNIFORM;
        else if (!strcmp(t->s, "varying")) q |= Q_VARYING;
        else if (!strcmp(t->s, "invariant") || is_precision_word(t->s)) ;
        else if (!strcmp(t->s, "in")) q |= Q_IN;
        else if (!strcmp(t->s, "out")) q |= Q_OUT;
        else if (!strcmp(t->s, "inout")) q |= Q_INOUT;
        else break;
        p->i++;
    }
    return q;
}

/* parse declarators after type: name [ [n] ] [= init] {, ...} ; */
static Node *declaration_rest(P *p, GType t, int qual, int global)
{
    Node *d = mk(p, S_DECL);
    d->ts.t = t; d->qual = qual;
    if (is_op(cur(p), ";")) { p->i++; return d; } /* bare struct decl */
    do {
        if (cur(p)->t != TK_IDENT) cerr(p->C, cur(p)->line, "expected identifier, found '%s'", cur(p)->s);
        Node *v = mk(p, D_VAR);
        v->name = cur(p)->s; v->ts.t = t; v->qual = qual; p->i++;
        if (accept(p, "[")) {
            if (is_op(cur(p), "]")) v->ts.t.arr = -1;   /* size from initializer */
            else v->ts.t.arr = const_int(p, expr(p));
            expect(p, "]");
        }
        if (accept(p, "=")) v->init = assign_expr(p);
        if (v->ts.t.arr < 0) v->ts.t.arr = v->init && v->init->k == N_CTOR ? v->init->ts.t.arr : 1;
        if ((qual & Q_CONST) && t.kind == GK_INT && !t.mat && t.vec == 1 && v->init) {
            g_consts = realloc(g_consts, (g_nconsts + 1) * sizeof(Node *));
            g_consts[g_nconsts++] = v;
        }
        nl_push(&d->list, v);
    } while (accept(p, ","));
    (void)global;
    expect(p, ";");
    return d;
}

static Node *block(P *p)
{
    Node *b = mk(p, S_BLOCK);
    expect(p, "{");
    while (!accept(p, "}")) {
        if (cur(p)->t == TK_EOF) cerr(p->C, cur(p)->line, "unexpected end of file");
        nl_push(&b->list, statement(p));
    }
    return b;
}

static int is_decl_start(P *p)
{
    Tok *t = cur(p);
    if (t->t != TK_IDENT) return 0;
    if (!strcmp(t->s, "const") || is_precision_word(t->s) || !strcmp(t->s, "struct")) return 1;
    GType g;
    if (type_of_name(p->C, t->s, &g)) {
        Tok *n = peekn(p, 1);
        if (n->t == TK_IDENT) return 1;
        if (is_op(n, "[")) {   /* float[3] x  vs  float[3](..) */
            int j = 2; while (!is_op(peekn(p, j), "]") && peekn(p, j)->t != TK_EOF) j++;
            return peekn(p, j + 1)->t == TK_IDENT;
        }
    }
    return 0;
}

static Node *simple_or_decl(P *p)
{
    if (is_decl_start(p)) {
        int q = parse_qualifiers(p);
        GType t = parse_type(p);
        if (accept(p, "[")) { t.arr = const_int(p, expr(p)); expect(p, "]"); }
        return declaration_rest(p, t, q, 0);
    }
    Node *s = mk(p, S_EXPR);
    s->a = expr(p);
    expect(p, ";");
    return s;
}

static Node *statement(P *p)
{
    Tok *t = cur(p);
    Node *s;
    if (is_op(t, "{")) return block(p);
    if (is_op(t, ";")) { p->i++; return mk(p, S_EMPTY); }
    if (t->t == TK_IDENT) {
        if (!strcmp(t->s, "if")) {
            s = mk(p, S_IF); p->i++; expect(p, "("); s->a = expr(p); expect(p, ")");
            s->b = statement(p);
            if (is_id(cur(p), "else")) { p->i++; s->c = statement(p); }
            return s;
        }
        if (!strcmp(t->s, "for")) {
            s = mk(p, S_FOR); p->i++; expect(p, "(");
            if (accept(p, ";")) s->a = NULL; else s->a = simple_or_decl(p);
            if (!is_op(cur(p), ";")) s->b = expr(p);
            expect(p, ";");
            if (!is_op(cur(p), ")")) s->c = expr(p);
            expect(p, ")");
            s->d = statement(p);
            return s;
        }
        if (!strcmp(t->s, "while")) {
            s = mk(p, S_WHILE); p->i++; expect(p, "("); s->a = expr(p); expect(p, ")"); s->b = statement(p);
            return s;
        }
        if (!strcmp(t->s, "do")) {
            s = mk(p, S_DO); p->i++; s->b = statement(p);
            if (!is_id(cur(p), "while")) cerr(p->C, cur(p)->line, "expected while");
            p->i++; expect(p, "("); s->a = expr(p); expect(p, ")"); expect(p, ";");
            return s;
        }
        if (!strcmp(t->s, "return")) {
            s = mk(p, S_RETURN); p->i++;
            if (!is_op(cur(p), ";")) s->a = expr(p);
            expect(p, ";"); return s;
        }
        if (!strcmp(t->s, "break")) { s = mk(p, S_BREAK); p->i++; expect(p, ";"); return s; }
        if (!strcmp(t->s, "continue")) { s = mk(p, S_CONTINUE); p->i++; expect(p, ";"); return s; }
        if (!strcmp(t->s, "discard")) { s = mk(p, S_DISCARD); p->i++; expect(p, ";"); return s; }
    }
    return simple_or_decl(p);
}

static Node *external(P *p)
{
    Tok *t = cur(p);
    if (is_id(t, "precision")) {
        while (!is_op(cur(p), ";") && cur(p)->t != TK_EOF) p->i++;
        accept(p, ";");
        return mk(p, D_PRECISION);
    }
    if (is_id(t, "invariant") && peekn(p, 1)->t == TK_IDENT && is_op(peekn(p, 2), ";")) {
        p->i += 3; return mk(p, D_PRECISION);
    }
    if (is_op(t, ";")) { p->i++; return mk(p, D_PRECISION); }
    int q = parse_qualifiers(p);
    GType ty = parse_type(p);
    if (cur(p)->t == TK_IDENT && is_op(peekn(p, 1), "(")) {
        Node *f = mk(p, D_FUNC);
        f->ts.t = ty; f->name = cur(p)->s; p->i += 2;
        if (is_id(cur(p), "void") && is_op(peekn(p, 1), ")")) p->i++;
        if (!is_op(cur(p), ")")) {
            do {
                Node *prm = mk(p, D_VAR);
                prm->qual = parse_qualifiers(p);
                prm->ts.t = parse_type(p);
                if (cur(p)->t == TK_IDENT) { prm->name = cur(p)->s; p->i++; }
                else prm->name = "";
                if (accept(p, "[")) { prm->ts.t.arr = const_int(p, expr(p)); expect(p, "]"); }
                nl_push(&f->list, prm);
            } while (accept(p, ","));
        }
        expect(p, ")");
        if (accept(p, ";")) f->body = NULL;
        else f->body = block(p);
        return f;
    }
    if (accept(p, "[")) { ty.arr = const_int(p, expr(p)); expect(p, "]"); }
    return declaration_rest(p, ty, q, 1);
}

Node *glsl_parse(Compiler *C, TokVec *toks)
{
    P p = { C, toks->v, 0, toks->n };
    g_consts = NULL; g_nconsts = 0;
    Node *root = mk(&p, S_BLOCK);
    while (cur(&p)->t != TK_EOF) nl_push(&root->list, external(&p));
    free(g_consts); g_consts = NULL; g_nconsts = 0;
    return root;
}

int glsl_tsize_c(Compiler *C, GType t) { return tsize(C, t); }
int glsl_type_by_name(Compiler *C, const char *s, GType *t) { return type_of_name(C, s, t); }
