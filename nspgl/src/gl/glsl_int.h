#ifndef GLSL_INT_H
#define GLSL_INT_H
#include "glsl.h"
#include <setjmp.h>
#include <stddef.h>

/* ---------- arena ---------- */
typedef struct ABlock { struct ABlock *next; size_t used, cap; char data[]; } ABlock;
typedef struct Arena { ABlock *head; } Arena;
void *ar_alloc(Arena *a, size_t n);
char *ar_strdup(Arena *a, const char *s, int n);
void ar_free(Arena *a);

/* ---------- tokens ---------- */
enum { TK_EOF, TK_IDENT, TK_NUM, TK_OP };
typedef struct Tok { int t; int line; int isfloat; double num; const char *s; } Tok;

typedef struct TokVec { Tok *v; int n, cap; } TokVec;
void tv_push(TokVec *tv, Tok t);

/* ---------- compile context ---------- */
typedef struct Compiler Compiler;
void cerr(Compiler *C, int line, const char *fmt, ...);
void clog_append(Compiler *C, const char *fmt, ...);

int glsl_preprocess(Compiler *C, const char *src, TokVec *out);

/* ---------- AST ---------- */
enum {
    N_NUM, N_IDENT, N_BIN, N_UN, N_ASSIGN, N_CALL, N_FIELD, N_INDEX, N_COND,
    N_PREINC, N_PREDEC, N_POSTINC, N_POSTDEC, N_COMMA, N_CTOR,
    S_BLOCK, S_DECL, S_EXPR, S_IF, S_FOR, S_WHILE, S_DO, S_RETURN, S_BREAK,
    S_CONTINUE, S_DISCARD, S_EMPTY,
    D_FUNC, D_VAR, D_PRECISION, D_STRUCT
};

typedef struct Node Node;
typedef struct NList { Node **v; int n, cap; } NList;

typedef struct TypeSpec {
    GType t;           /* arr = 0 here; array size in decl */
    Node *arrexpr;     /* for `float[3]` style (rare) */
} TypeSpec;

enum { Q_NONE = 0, Q_CONST = 1, Q_ATTRIBUTE = 2, Q_UNIFORM = 4, Q_VARYING = 8,
       Q_IN = 16, Q_OUT = 32, Q_INOUT = 48 };

struct Node {
    int k, line;
    int op;             /* operator char code / id */
    const char *name;
    double num; int isfloat, isbool;
    Node *a, *b, *c, *d;
    NList list;         /* args / statements / declarators / params */
    TypeSpec ts;        /* declared type / ctor type / function return type */
    int qual;
    Node *arrsize;      /* declarator array size */
    Node *init;         /* declarator initializer */
    Node *body;         /* function body */
};

/* struct definitions */
typedef struct SField { const char *name; GType t; int off; } SField;
typedef struct SDef { const char *name; SField f[32]; int nf; int size; } SDef;

void nl_push(NList *l, Node *n);
Node *glsl_parse(Compiler *C, TokVec *toks);

/* ---------- interpreter opcodes ---------- */
enum {
    OP_NOP, OP_MOV, OP_SWZ, OP_SCAT,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_NEG,
    OP_MOD, OP_MIN, OP_MAX, OP_POW, OP_ATAN2, OP_STEP,
    OP_CLAMP, OP_MIX, OP_SSTEP,
    OP_UFN,
    OP_DOT, OP_CROSS, OP_LEN, OP_DIST, OP_NORM, OP_REFLECT, OP_REFRACT, OP_FFWD,
    OP_MATVEC, OP_VECMAT, OP_MATMAT,
    OP_LT, OP_LE, OP_GT, OP_GE, OP_EQC, OP_NEC, OP_EQ, OP_NE,
    OP_ANY, OP_ALL, OP_AND, OP_OR, OP_XOR,
    OP_JMP, OP_JZ, OP_JNZ, OP_KILL,
    OP_TEX, OP_TEXPROJ, OP_TEXCUBE,
    OP_IDXLD, OP_IDXST, OP_MATDIAG, OP_MATRESIZE, OP_END
};
enum {
    UF_SIN, UF_COS, UF_TAN, UF_ASIN, UF_ACOS, UF_ATAN, UF_EXP, UF_LOG, UF_EXP2,
    UF_LOG2, UF_SQRT, UF_RSQRT, UF_ABS, UF_SIGN, UF_FLOOR, UF_CEIL, UF_FRACT,
    UF_RAD, UF_DEG, UF_NOT, UF_TRUNC, UF_TOBOOL
};
/* flags: broadcast scalar operands */
#define FL_SA 1
#define FL_SB 2
#define FL_SC 4

struct Compiler {
    Arena ar;
    jmp_buf jb;
    char *log; int loglen, logcap;
    int stage;
    int errors;
    /* defines */
    struct Macro { const char *name; int nparams; const char *params[16]; Tok *body; int nbody; int funclike; } *macros;
    int nmacros, capmacros;
    /* struct table */
    SDef structs[16]; int nstructs;
};

#endif
