/* GLSL bytecode interpreter */
#include "glsl_int.h"
#include <math.h>
#include "fastmath.h"
#include <string.h>

#define LOOP_LIMIT 200000

static inline float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

int glsl_run(const GShader *s, float *m, GSampleFn sampler, void *user)
{
    const GOp *code = s->code;
    int pc = 0, guard = 0;
    for (;;) {
        const GOp *o = &code[pc++];
        int n = o->n, i;
        float *d = m + o->d;
        switch (o->op) {
        case OP_NOP: break;
        case OP_END: return 0;
        case OP_MOV: {
            const float *a = m + o->a;
            if (n == 1) { d[0] = a[0]; break; }
            if (o->fl & FL_SA) { float v = a[0]; for (i = 0; i < n; i++) d[i] = v; }
            else if (d != a) for (i = 0; i < n; i++) d[i] = a[i];
            break; }
        case OP_SWZ: {
            const float *a = m + o->a; uint32_t c = (uint32_t)o->c;
            float t[4];
            for (i = 0; i < n; i++) t[i] = a[(c >> (8 * i)) & 255];
            for (i = 0; i < n; i++) d[i] = t[i];
            break; }
        case OP_SCAT: {
            const float *a = m + o->a; uint32_t c = (uint32_t)o->c;
            float t[4];
            for (i = 0; i < n; i++) t[i] = a[i];
            for (i = 0; i < n; i++) d[(c >> (8 * i)) & 255] = t[i];
            break; }
#define BIN(OPC, EXPR) case OPC: { const float *a = m + o->a, *b = m + o->b; \
            if (n == 1) { float x = a[0], y = b[0]; d[0] = (EXPR); break; } \
            int sa = (o->fl & FL_SA) ? 0 : 1, sb = (o->fl & FL_SB) ? 0 : 1; \
            for (i = 0; i < n; i++) { float x = a[i * sa], y = b[i * sb]; d[i] = (EXPR); } break; }
        BIN(OP_ADD, x + y)
        BIN(OP_SUB, x - y)
        BIN(OP_MUL, x * y)
        BIN(OP_DIV, y != 0.0f ? x / y : (x == 0.0f ? 0.0f : (x > 0 ? 3.0e38f : -3.0e38f)))
        BIN(OP_MOD, y != 0.0f ? x - y * fm_floor(x / y) : 0.0f)
        BIN(OP_MIN, y < x ? y : x)
        BIN(OP_MAX, y > x ? y : x)
        BIN(OP_POW, fm_pow(x, y))
        BIN(OP_ATAN2, fm_atan2(x, y))
        BIN(OP_STEP, y < x ? 0.0f : 1.0f)
        BIN(OP_LT, x < y)
        BIN(OP_LE, x <= y)
        BIN(OP_GT, x > y)
        BIN(OP_GE, x >= y)
        BIN(OP_EQC, x == y)
        BIN(OP_NEC, x != y)
        BIN(OP_AND, (x != 0.0f) && (y != 0.0f))
        BIN(OP_OR, (x != 0.0f) || (y != 0.0f))
        BIN(OP_XOR, (x != 0.0f) != (y != 0.0f))
        case OP_NEG: { const float *a = m + o->a; for (i = 0; i < n; i++) d[i] = -a[i]; break; }
        case OP_CLAMP: {
            const float *a = m + o->a, *b = m + o->b, *c = m + o->c;
            int sb = (o->fl & FL_SB) ? 0 : 1, sc = (o->fl & FL_SC) ? 0 : 1;
            for (i = 0; i < n; i++) { float x = a[i], lo = b[i * sb], hi = c[i * sc]; d[i] = x < lo ? lo : x > hi ? hi : x; }
            break; }
        case OP_MIX: {
            const float *a = m + o->a, *b = m + o->b, *c = m + o->c;
            int sc = (o->fl & FL_SC) ? 0 : 1;
            for (i = 0; i < n; i++) { float t = c[i * sc]; d[i] = a[i] + (b[i] - a[i]) * t; }
            break; }
        case OP_SSTEP: {
            const float *a = m + o->a, *b = m + o->b, *c = m + o->c;
            int sa = (o->fl & FL_SA) ? 0 : 1, sb = (o->fl & FL_SB) ? 0 : 1;
            for (i = 0; i < n; i++) {
                float e0 = a[i * sa], e1 = b[i * sb], den = e1 - e0;
                float t = den != 0.0f ? clampf((c[i] - e0) / den, 0, 1) : (c[i] < e0 ? 0.0f : 1.0f);
                d[i] = t * t * (3.0f - 2.0f * t);
            }
            break; }
        case OP_UFN: {
            const float *a = m + o->a;
            switch (o->c) {
#define UF(ID, EXPR) case ID: for (i = 0; i < n; i++) { float x = a[i]; d[i] = (EXPR); } break;
            UF(UF_SIN, fm_sin(x)) UF(UF_COS, fm_cos(x)) UF(UF_TAN, fm_tan(x))
            UF(UF_ASIN, fm_asin(x)) UF(UF_ACOS, fm_acos(x)) UF(UF_ATAN, fm_atan(x))
            UF(UF_EXP, fm_exp(x)) UF(UF_LOG, fm_log(x)) UF(UF_EXP2, fm_exp2(x))
            UF(UF_LOG2, fm_log2(x)) UF(UF_SQRT, fm_sqrt(x))
            UF(UF_RSQRT, (int32_t)f2u(x) > 0 ? fm_rsqrt(x) : 3.0e38f) UF(UF_ABS, u2f(f2u(x) & 0x7FFFFFFFu))
            UF(UF_SIGN, (float)((x > 0) - (x < 0))) UF(UF_FLOOR, fm_floor(x)) UF(UF_CEIL, -fm_floor(-x))
            UF(UF_FRACT, x - fm_floor(x)) UF(UF_RAD, x * 0.017453292519943295f) UF(UF_DEG, x * 57.29577951308232f)
            UF(UF_NOT, x == 0.0f) UF(UF_TRUNC, (float)(int)x) UF(UF_TOBOOL, x != 0.0f)
            }
            break; }
        case OP_DOT: { const float *a = m + o->a, *b = m + o->b; float s = 0; for (i = 0; i < n; i++) s += a[i] * b[i]; d[0] = s; break; }
        case OP_LEN: { const float *a = m + o->a; float s = 0; for (i = 0; i < n; i++) s += a[i] * a[i]; d[0] = fm_sqrt(s); break; }
        case OP_DIST: { const float *a = m + o->a, *b = m + o->b; float s = 0; for (i = 0; i < n; i++) { float t = a[i] - b[i]; s += t * t; } d[0] = fm_sqrt(s); break; }
        case OP_NORM: {
            const float *a = m + o->a; float s = 0;
            for (i = 0; i < n; i++) s += a[i] * a[i];
            s = s > 0 ? fm_rsqrt(s) : 0.0f;
            for (i = 0; i < n; i++) d[i] = a[i] * s;
            break; }
        case OP_CROSS: {
            const float *a = m + o->a, *b = m + o->b;
            float x = a[1] * b[2] - a[2] * b[1], y = a[2] * b[0] - a[0] * b[2], z = a[0] * b[1] - a[1] * b[0];
            d[0] = x; d[1] = y; d[2] = z; break; }
        case OP_REFLECT: {
            const float *I = m + o->a, *N = m + o->b; float k = 0;
            for (i = 0; i < n; i++) k += N[i] * I[i];
            for (i = 0; i < n; i++) d[i] = I[i] - 2.0f * k * N[i];
            break; }
        case OP_REFRACT: {
            const float *I = m + o->a, *N = m + o->b; float eta = m[o->c], dn = 0;
            for (i = 0; i < n; i++) dn += N[i] * I[i];
            float k = 1.0f - eta * eta * (1.0f - dn * dn);
            if (k < 0) for (i = 0; i < n; i++) d[i] = 0;
            else { float f = eta * dn + fm_sqrt(k); for (i = 0; i < n; i++) d[i] = eta * I[i] - f * N[i]; }
            break; }
        case OP_FFWD: {
            const float *N = m + o->a, *I = m + o->b, *R = m + o->c; float dd = 0;
            for (i = 0; i < n; i++) dd += R[i] * I[i];
            for (i = 0; i < n; i++) d[i] = dd < 0 ? N[i] : -N[i];
            break; }
        case OP_MATVEC: {   /* d = M * v, column-major */
            const float *M = m + o->a, *v = m + o->b; float t[4];
            for (int r = 0; r < n; r++) { float s = 0; for (int c = 0; c < n; c++) s += M[c * n + r] * v[c]; t[r] = s; }
            for (i = 0; i < n; i++) d[i] = t[i];
            break; }
        case OP_VECMAT: {   /* d = v * M */
            const float *v = m + o->a, *M = m + o->b; float t[4];
            for (int c = 0; c < n; c++) { float s = 0; for (int r = 0; r < n; r++) s += v[r] * M[c * n + r]; t[c] = s; }
            for (i = 0; i < n; i++) d[i] = t[i];
            break; }
        case OP_MATMAT: {
            const float *A = m + o->a, *B = m + o->b; float t[16];
            for (int c = 0; c < n; c++) for (int r = 0; r < n; r++) {
                float s = 0; for (int k = 0; k < n; k++) s += A[k * n + r] * B[c * n + k];
                t[c * n + r] = s;
            }
            for (i = 0; i < n * n; i++) d[i] = t[i];
            break; }
        case OP_EQ: case OP_NE: {
            const float *a = m + o->a, *b = m + o->b; int eq = 1;
            for (i = 0; i < n; i++) if (a[i] != b[i]) { eq = 0; break; }
            d[0] = (o->op == OP_EQ) ? eq : !eq;
            break; }
        case OP_ANY: { const float *a = m + o->a; float r = 0; for (i = 0; i < n; i++) if (a[i] != 0) r = 1; d[0] = r; break; }
        case OP_ALL: { const float *a = m + o->a; float r = 1; for (i = 0; i < n; i++) if (a[i] == 0) r = 0; d[0] = r; break; }
        case OP_JMP:
            if (o->a < pc && ++guard > LOOP_LIMIT) return -1;
            pc = o->a; break;
        case OP_JZ: if (m[o->a] == 0.0f) { if (o->b < pc && ++guard > LOOP_LIMIT) return -1; pc = o->b; } break;
        case OP_JNZ: if (m[o->a] != 0.0f) { if (o->b < pc && ++guard > LOOP_LIMIT) return -1; pc = o->b; } break;
        case OP_KILL: return 1;
        case OP_TEX: case OP_TEXPROJ: case OP_TEXCUBE: {
            float crd[3]; const float *c = m + o->b;
            float bias = o->c >= 0 ? m[o->c] : 0.0f;
            if (o->op == OP_TEXPROJ) { float q = c[n - 1]; if (q == 0) q = 1e-20f; crd[0] = c[0] / q; crd[1] = c[1] / q; crd[2] = 0; }
            else { crd[0] = c[0]; crd[1] = c[1]; crd[2] = o->op == OP_TEXCUBE ? c[2] : 0; }
            if (sampler) sampler(user, (int)m[o->a], o->op == OP_TEXCUBE, crd, bias, d);
            else { d[0] = d[1] = d[2] = 0; d[3] = 1; }
            break; }
        case OP_IDXLD: {
            int k = (int)m[o->b]; if (k < 0) k = 0; if (k >= o->c) k = o->c - 1;
            const float *a = m + o->a + k * n;
            for (i = 0; i < n; i++) d[i] = a[i];
            break; }
        case OP_IDXST: {
            int k = (int)m[o->b]; if (k < 0 || k >= o->c) break;
            const float *a = m + o->a; float *dd = d + k * n;
            for (i = 0; i < n; i++) dd[i] = a[i];
            break; }
        case OP_MATDIAG: {
            float v = m[o->a];
            for (i = 0; i < n * n; i++) d[i] = 0;
            for (i = 0; i < n; i++) d[i * n + i] = v;
            break; }
        case OP_MATRESIZE: {
            const float *a = m + o->a; int sn = o->c; float t[16];
            for (int c = 0; c < n; c++) for (int r = 0; r < n; r++)
                t[c * n + r] = (c < sn && r < sn) ? a[c * sn + r] : (c == r ? 1.0f : 0.0f);
            for (i = 0; i < n * n; i++) d[i] = t[i];
            break; }
        default:
            return -1;
        }
    }
}
