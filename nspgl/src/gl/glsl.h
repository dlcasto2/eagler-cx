/* Tiny GLSL ES 1.00 compiler + interpreter for nspGL.
 * Shaders are compiled to a flat register-machine program; every user
 * function is inlined (GLSL forbids recursion) so the whole shader is one
 * straight piece of code operating on a fixed float memory block. */
#ifndef GLSL_H
#define GLSL_H
#include <stdint.h>

enum { GK_VOID, GK_FLOAT, GK_INT, GK_BOOL, GK_SAMPLER2D, GK_SAMPLERCUBE, GK_STRUCT };

typedef struct GType {
    uint8_t kind;   /* GK_* */
    uint8_t vec;    /* components of a vector (1..4); for matrices = rows */
    uint8_t mat;    /* 0, or number of columns (2..4) */
    uint8_t pad;
    int16_t arr;    /* 0 = not an array, else element count */
    int16_t sid;    /* struct id when kind == GK_STRUCT */
} GType;

enum { GS_NONE, GS_ATTRIBUTE, GS_UNIFORM, GS_VARYING, GS_BUILTIN };

typedef struct GVar {
    char name[48];
    GType type;
    int off;       /* float offset in the shader memory block */
    int storage;   /* GS_* */
} GVar;

typedef struct GOp {
    uint8_t op, n, fl, pad;
    int32_t d, a, b, c;
} GOp;

/* sampler callback: unit, coords (s,t[,r]), lod bias, out rgba */
typedef void (*GSampleFn)(void *user, int unit, int cube, const float *coord, float bias, float *out);

typedef struct GShader {
    int stage;            /* 0 = vertex, 1 = fragment */
    int ok;
    char *log;            /* compile log (malloc'd) */
    GOp *code; int ncode;
    float *init; int memsize;  /* initial memory (constants) */
    GVar *vars; int nvars;
    int off_position, off_pointsize, off_fragcolor, off_fragcoord,
        off_frontfacing, off_pointcoord;
    int writes_pointsize;
    int has_discard;
    int nsamplers_used;
} GShader;

GShader *glsl_compile(const char *src, int stage);
void glsl_free(GShader *s);
/* run program; returns 1 if discarded, 0 otherwise, -1 runaway */
int glsl_run(const GShader *s, float *mem, GSampleFn sampler, void *user);
int glsl_type_size(GType t);   /* floats per element (incl. array) */
const char *glsl_type_name(GType t);
#endif
