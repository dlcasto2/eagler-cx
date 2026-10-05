/* WebGLRenderingContext binding for QuickJS over the SGL software renderer */
#include "webgl_js.h"
#include "canvas2d.h"
#include "../gl/sgl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

enum { OK_BUFFER, OK_TEXTURE, OK_PROGRAM, OK_SHADER, OK_FRAMEBUFFER, OK_RENDERBUFFER, OK_COUNT };
static const char *ok_names[OK_COUNT] = { "WebGLBuffer", "WebGLTexture", "WebGLProgram", "WebGLShader", "WebGLFramebuffer", "WebGLRenderbuffer" };

static JSClassID gl_cid, obj_cid, loc_cid;
static JSValue gl_proto, obj_proto[OK_COUNT], loc_proto;
static int quality_pct = 50;

typedef struct GLObj { int kind, id; struct GLCtx *owner; } GLObj;
typedef struct GLLoc { int prog, uidx, elem; } GLLoc;
typedef struct GLCtx {
    SGL *g;
    int vw, vh;
    int alpha, depth, antialias, premul, preserve;
    JSValue *objs[OK_COUNT]; int nobjs[OK_COUNT];
} GLCtx;

void webgl_set_quality(int pct) { quality_pct = pct < 10 ? 10 : pct > 100 ? 100 : pct; }

static void real_size(int vw, int vh, int *rw, int *rh)
{
    float s = 1.0f;
    if (vw > SCR_MAXW) s = (float)SCR_MAXW / vw;
    if (vh * s > SCR_MAXH) s = (float)SCR_MAXH / vh;
    s *= quality_pct / 100.0f;
    *rw = (int)(vw * s + 0.5f); *rh = (int)(vh * s + 0.5f);
    if (*rw < 1) *rw = 1;
    if (*rh < 1) *rh = 1;
}

static GLCtx *getctx(JSContext *ctx, JSValueConst this_val) { return JS_GetOpaque2(ctx, this_val, gl_cid); }

static JSValue wrap_obj(JSContext *ctx, GLCtx *c, int kind, int id)
{
    if (id <= 0) return JS_NULL;
    if (id < c->nobjs[kind] && !JS_IsUndefined(c->objs[kind][id])) return JS_DupValue(ctx, c->objs[kind][id]);
    JSValue o = JS_NewObjectProtoClass(ctx, obj_proto[kind], obj_cid);
    GLObj *g = malloc(sizeof(GLObj));
    g->kind = kind; g->id = id; g->owner = c;
    JS_SetOpaque(o, g);
    if (id >= c->nobjs[kind]) {
        int n = id * 2 + 8;
        c->objs[kind] = realloc(c->objs[kind], n * sizeof(JSValue));
        for (int i = c->nobjs[kind]; i < n; i++) c->objs[kind][i] = JS_UNDEFINED;
        c->nobjs[kind] = n;
    }
    c->objs[kind][id] = JS_DupValue(ctx, o);
    return o;
}
static int unwrap(JSContext *ctx, JSValueConst v, int kind)
{
    if (JS_IsNull(v) || JS_IsUndefined(v)) return 0;
    GLObj *g = JS_GetOpaque(v, obj_cid);
    (void)ctx;
    if (!g || g->kind != kind) return -1;
    return g->id;
}
static void drop_obj(JSContext *ctx, GLCtx *c, int kind, int id)
{
    if (id > 0 && id < c->nobjs[kind]) { JS_FreeValue(ctx, c->objs[kind][id]); c->objs[kind][id] = JS_UNDEFINED; }
}

static int ai(JSContext *ctx, JSValueConst v) { int32_t r = 0; JS_ToInt32(ctx, &r, v); return r; }
static double ad(JSContext *ctx, JSValueConst v) { double r = 0; JS_ToFloat64(ctx, &r, v); return r; }
#define A(i) (i < argc ? argv[i] : JS_UNDEFINED)
#define I(i) ai(ctx, A(i))
#define F(i) ((float)ad(ctx, A(i)))
#define C GLCtx *c = getctx(ctx, this_val); if (!c) return JS_EXCEPTION; SGL *g = c->g; (void)g;
static JSValue new_ta(JSContext *ctx, JSValue ab, JSTypedArrayEnum t)
{
    JSValueConst args[3] = { ab, JS_UNDEFINED, JS_UNDEFINED };
    return JS_NewTypedArray(ctx, 3, args, t);
}
#define FN(name) static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)

/* get raw bytes of ArrayBuffer / view */
static uint8_t *get_bytes(JSContext *ctx, JSValueConst v, size_t *len, size_t *bpe)
{
    size_t off, l, b = 1;
    if (bpe) *bpe = 1;
    JSValue buf = JS_GetTypedArrayBuffer(ctx, v, &off, &l, &b);
    if (!JS_IsException(buf)) {
        size_t sz; uint8_t *p = JS_GetArrayBuffer(ctx, &sz, buf);
        JS_FreeValue(ctx, buf);
        if (!p) return NULL;
        *len = l; if (bpe) *bpe = b;
        return p + off;
    }
    JS_FreeValue(ctx, JS_GetException(ctx));
    size_t sz;
    uint8_t *p = JS_GetArrayBuffer(ctx, &sz, v);
    if (p) { *len = sz; return p; }
    JS_FreeValue(ctx, JS_GetException(ctx));
    /* DataView */
    JSValue bv = JS_GetPropertyStr(ctx, v, "buffer");
    if (JS_IsObject(bv)) {
        p = JS_GetArrayBuffer(ctx, &sz, bv);
        if (p) {
            size_t o2 = (size_t)ai(ctx, JS_GetPropertyStr(ctx, v, "byteOffset"));
            size_t l2 = (size_t)ai(ctx, JS_GetPropertyStr(ctx, v, "byteLength"));
            JS_FreeValue(ctx, bv);
            *len = l2; return p + o2;
        }
        JS_FreeValue(ctx, JS_GetException(ctx));
    }
    JS_FreeValue(ctx, bv);
    return NULL;
}

/* typed array -> floats (malloc'd); handles Float32Array, Int32Array, plain arrays */
static float *get_floats(JSContext *ctx, JSValueConst v, int *n)
{
    size_t len, bpe;
    *n = 0;
    uint8_t *p = get_bytes(ctx, v, &len, &bpe);
    if (p) {
        /* need element type: check constructor name */
        JSValue cn = JS_GetPropertyStr(ctx, v, "constructor");
        JSValue nm = JS_GetPropertyStr(ctx, cn, "name");
        const char *s = JS_ToCString(ctx, nm);
        int cnt = (int)(len / bpe);
        float *f = malloc(sizeof(float) * (cnt + 1));
        for (int i = 0; i < cnt; i++) {
            if (s && !strcmp(s, "Float32Array")) memcpy(&f[i], p + i * 4, 4);
            else if (s && !strcmp(s, "Float64Array")) { double d; memcpy(&d, p + i * 8, 8); f[i] = (float)d; }
            else if (s && !strcmp(s, "Int32Array")) { int32_t x; memcpy(&x, p + i * 4, 4); f[i] = (float)x; }
            else if (s && !strcmp(s, "Uint32Array")) { uint32_t x; memcpy(&x, p + i * 4, 4); f[i] = (float)x; }
            else if (s && !strcmp(s, "Int16Array")) { int16_t x; memcpy(&x, p + i * 2, 2); f[i] = x; }
            else if (s && !strcmp(s, "Uint16Array")) { uint16_t x; memcpy(&x, p + i * 2, 2); f[i] = x; }
            else if (s && !strcmp(s, "Int8Array")) f[i] = (int8_t)p[i];
            else f[i] = p[i];
        }
        JS_FreeCString(ctx, s); JS_FreeValue(ctx, nm); JS_FreeValue(ctx, cn);
        *n = cnt;
        return f;
    }
    if (JS_IsArray(ctx, v)) {
        int cnt = ai(ctx, JS_GetPropertyStr(ctx, v, "length"));
        float *f = malloc(sizeof(float) * (cnt + 1));
        for (int i = 0; i < cnt; i++) { JSValue e = JS_GetPropertyUint32(ctx, v, i); f[i] = (float)ad(ctx, e); JS_FreeValue(ctx, e); }
        *n = cnt;
        return f;
    }
    return NULL;
}

/* ---------------- state ---------------- */
FN(m_viewport) { C sgl_viewport(g, I(0), I(1), I(2), I(3)); return JS_UNDEFINED; }
FN(m_scissor) { C sgl_scissor(g, I(0), I(1), I(2), I(3)); return JS_UNDEFINED; }
FN(m_clearColor) { C g->clear_color[0] = F(0); g->clear_color[1] = F(1); g->clear_color[2] = F(2); g->clear_color[3] = F(3); return JS_UNDEFINED; }
FN(m_clearDepth) { C g->clear_depth = F(0); return JS_UNDEFINED; }
FN(m_clearStencil) { C g->clear_stencil = I(0); return JS_UNDEFINED; }
FN(m_clear) { C sgl_clear(g, I(0)); return JS_UNDEFINED; }
FN(m_enable) { C sgl_enable(g, I(0), 1); return JS_UNDEFINED; }
FN(m_disable) { C sgl_enable(g, I(0), 0); return JS_UNDEFINED; }
FN(m_isEnabled) { C return JS_NewBool(ctx, sgl_is_enabled(g, I(0))); }
FN(m_depthFunc) { C g->depth_func = I(0); return JS_UNDEFINED; }
FN(m_depthMask) { C g->depth_mask = JS_ToBool(ctx, A(0)); return JS_UNDEFINED; }
FN(m_depthRange) { C g->depth_range[0] = F(0); g->depth_range[1] = F(1); return JS_UNDEFINED; }
FN(m_cullFace) { C g->cull_face = I(0); return JS_UNDEFINED; }
FN(m_frontFace) { C g->front_face = I(0); return JS_UNDEFINED; }
FN(m_blendFunc) { C g->bsrc_rgb = g->bsrc_a = I(0); g->bdst_rgb = g->bdst_a = I(1); return JS_UNDEFINED; }
FN(m_blendFuncSeparate) { C g->bsrc_rgb = I(0); g->bdst_rgb = I(1); g->bsrc_a = I(2); g->bdst_a = I(3); return JS_UNDEFINED; }
FN(m_blendEquation) { C g->beq_rgb = g->beq_a = I(0); return JS_UNDEFINED; }
FN(m_blendEquationSeparate) { C g->beq_rgb = I(0); g->beq_a = I(1); return JS_UNDEFINED; }
FN(m_blendColor) { C g->blend_color[0] = F(0); g->blend_color[1] = F(1); g->blend_color[2] = F(2); g->blend_color[3] = F(3); return JS_UNDEFINED; }
FN(m_colorMask) { C for (int i = 0; i < 4; i++) g->color_mask[i] = JS_ToBool(ctx, A(i)); return JS_UNDEFINED; }
FN(m_lineWidth) { C g->line_width = F(0); return JS_UNDEFINED; }
FN(m_polygonOffset) { C g->po_factor = F(0); g->po_units = F(1); return JS_UNDEFINED; }
FN(m_stencilFunc) { C g->stencil_func = I(0); g->stencil_ref = I(1); g->stencil_mask = I(2); return JS_UNDEFINED; }
FN(m_stencilMask) { C g->stencil_wmask = I(0); return JS_UNDEFINED; }
FN(m_noop) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
FN(m_pixelStorei)
{
    C int p = I(0), v = I(1);
    if (p == GL_UNPACK_FLIP_Y_WEBGL) g->unpack_flip_y = v != 0;
    else if (p == GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL) g->unpack_premul = v != 0;
    else if (p == GL_UNPACK_ALIGNMENT) g->unpack_align = v;
    else if (p == GL_PACK_ALIGNMENT) g->pack_align = v;
    return JS_UNDEFINED;
}
FN(m_getError) { C int e = g->error; g->error = 0; return JS_NewInt32(ctx, e); }
FN(m_isContextLost) { (void)this_val; (void)argc; (void)argv; return JS_FALSE; }
FN(m_activeTexture) { C int u = I(0) - GL_TEXTURE0; if (u < 0 || u >= SGL_MAX_UNITS) sgl_set_error(g, GL_INVALID_ENUM); else g->active_unit = u; return JS_UNDEFINED; }

/* ---------------- objects ---------------- */
#define CREATE(fname, kind, call) FN(fname) { C (void)argc; (void)argv; return wrap_obj(ctx, c, kind, call); }
CREATE(m_createBuffer, OK_BUFFER, sgl_create_buffer(g))
CREATE(m_createTexture, OK_TEXTURE, sgl_create_texture(g))
CREATE(m_createProgram, OK_PROGRAM, sgl_create_program(g))
CREATE(m_createFramebuffer, OK_FRAMEBUFFER, sgl_create_framebuffer(g))
CREATE(m_createRenderbuffer, OK_RENDERBUFFER, sgl_create_renderbuffer(g))
FN(m_createShader) { C return wrap_obj(ctx, c, OK_SHADER, sgl_create_shader(g, I(0))); }
#define DELETE(fname, kind, call) FN(fname) { C int id = unwrap(ctx, A(0), kind); if (id > 0) { call(g, id); drop_obj(ctx, c, kind, id); } return JS_UNDEFINED; }
DELETE(m_deleteBuffer, OK_BUFFER, sgl_delete_buffer)
DELETE(m_deleteTexture, OK_TEXTURE, sgl_delete_texture)
DELETE(m_deleteProgram, OK_PROGRAM, sgl_delete_program)
DELETE(m_deleteShader, OK_SHADER, sgl_delete_shader)
DELETE(m_deleteFramebuffer, OK_FRAMEBUFFER, sgl_delete_framebuffer)
DELETE(m_deleteRenderbuffer, OK_RENDERBUFFER, sgl_delete_renderbuffer)
#define ISOBJ(fname, kind, tab) FN(fname) { C int id = unwrap(ctx, A(0), kind); return JS_NewBool(ctx, id > 0 && sgl_obj(&g->tab, id) != NULL); }
ISOBJ(m_isBuffer, OK_BUFFER, buffers)
ISOBJ(m_isTexture, OK_TEXTURE, textures)
ISOBJ(m_isProgram, OK_PROGRAM, programs)
ISOBJ(m_isShader, OK_SHADER, shaders)
ISOBJ(m_isFramebuffer, OK_FRAMEBUFFER, framebuffers)
ISOBJ(m_isRenderbuffer, OK_RENDERBUFFER, renderbuffers)

FN(m_bindBuffer) { C int id = unwrap(ctx, A(1), OK_BUFFER); if (id < 0) { sgl_set_error(g, GL_INVALID_OPERATION); return JS_UNDEFINED; } sgl_bind_buffer(g, I(0), id); return JS_UNDEFINED; }
FN(m_bufferData)
{
    C int target = I(0), usage = I(2);
    if (JS_IsNumber(A(1))) { sgl_buffer_data(g, target, NULL, I(1), usage); return JS_UNDEFINED; }
    size_t len; uint8_t *p = get_bytes(ctx, A(1), &len, NULL);
    if (!p) {
        if (JS_IsArray(ctx, A(1))) {   /* tolerate plain arrays as float data */
            int n; float *f = get_floats(ctx, A(1), &n);
            sgl_buffer_data(g, target, f, n * 4, usage); free(f);
        } else sgl_set_error(g, GL_INVALID_VALUE);
        return JS_UNDEFINED;
    }
    /* WebGL2-style (srcOffset, length) ignored */
    sgl_buffer_data(g, target, p, (int)len, usage);
    return JS_UNDEFINED;
}
FN(m_bufferSubData)
{
    C size_t len; uint8_t *p = get_bytes(ctx, A(2), &len, NULL);
    if (p) sgl_buffer_sub_data(g, I(0), I(1), p, (int)len);
    return JS_UNDEFINED;
}
FN(m_getBufferParameter)
{
    C int t = I(0), p = I(1);
    SGLBuffer *b = sgl_obj(&g->buffers, t == GL_ARRAY_BUFFER ? g->array_buffer : g->element_buffer);
    if (!b) return JS_NULL;
    if (p == 0x8764) return JS_NewInt32(ctx, b->size);
    if (p == 0x8765) return JS_NewInt32(ctx, b->usage);
    return JS_NULL;
}

/* ---------------- shaders / programs ---------------- */
FN(m_shaderSource)
{
    C int id = unwrap(ctx, A(0), OK_SHADER);
    const char *s = JS_ToCString(ctx, A(1));
    if (id > 0 && s) sgl_shader_source(g, id, s);
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
FN(m_getShaderSource) { C int id = unwrap(ctx, A(0), OK_SHADER); SGLShader *s = sgl_obj(&g->shaders, id); return s ? JS_NewString(ctx, s->src) : JS_NULL; }
FN(m_compileShader) { C int id = unwrap(ctx, A(0), OK_SHADER); if (id > 0) sgl_compile_shader(g, id); return JS_UNDEFINED; }
FN(m_getShaderParameter)
{
    C SGLShader *s = sgl_obj(&g->shaders, unwrap(ctx, A(0), OK_SHADER));
    if (!s) return JS_NULL;
    switch (I(1)) {
    case 0x8B81: return JS_NewBool(ctx, s->compile_ok);
    case 0x8B4F: return JS_NewInt32(ctx, s->type);
    case 0x8B80: return JS_NewBool(ctx, s->deleted);
    }
    return JS_NULL;
}
FN(m_getShaderInfoLog) { C SGLShader *s = sgl_obj(&g->shaders, unwrap(ctx, A(0), OK_SHADER)); return s ? JS_NewString(ctx, s->log ? s->log : "") : JS_NULL; }
FN(m_getShaderPrecisionFormat)
{
    (void)this_val;
    JSValue o = JS_NewObject(ctx);
    int t = I(1);
    int isint = t >= 0x8DF3;
    JS_SetPropertyStr(ctx, o, "rangeMin", JS_NewInt32(ctx, isint ? 31 : 127));
    JS_SetPropertyStr(ctx, o, "rangeMax", JS_NewInt32(ctx, isint ? 30 : 127));
    JS_SetPropertyStr(ctx, o, "precision", JS_NewInt32(ctx, isint ? 0 : 23));
    return o;
}
FN(m_attachShader) { C sgl_attach_shader(g, unwrap(ctx, A(0), OK_PROGRAM), unwrap(ctx, A(1), OK_SHADER)); return JS_UNDEFINED; }
FN(m_detachShader) { C sgl_detach_shader(g, unwrap(ctx, A(0), OK_PROGRAM), unwrap(ctx, A(1), OK_SHADER)); return JS_UNDEFINED; }
FN(m_getAttachedShaders)
{
    C SGLProgram *p = sgl_obj(&g->programs, unwrap(ctx, A(0), OK_PROGRAM));
    JSValue a = JS_NewArray(ctx); int n = 0;
    if (p && p->vs) JS_SetPropertyUint32(ctx, a, n++, wrap_obj(ctx, c, OK_SHADER, p->vs));
    if (p && p->fs) JS_SetPropertyUint32(ctx, a, n++, wrap_obj(ctx, c, OK_SHADER, p->fs));
    return a;
}
FN(m_bindAttribLocation)
{
    C const char *s = JS_ToCString(ctx, A(2));
    if (s) sgl_bind_attrib_location(g, unwrap(ctx, A(0), OK_PROGRAM), I(1), s);
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
FN(m_linkProgram) { C sgl_link_program(g, unwrap(ctx, A(0), OK_PROGRAM)); return JS_UNDEFINED; }
FN(m_useProgram) { C int id = unwrap(ctx, A(0), OK_PROGRAM); sgl_use_program(g, id < 0 ? 0 : id); return JS_UNDEFINED; }
FN(m_validateProgram) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
FN(m_getProgramParameter)
{
    C SGLProgram *p = sgl_obj(&g->programs, unwrap(ctx, A(0), OK_PROGRAM));
    if (!p) return JS_NULL;
    switch (I(1)) {
    case 0x8B82: return JS_NewBool(ctx, p->link_ok);
    case 0x8B83: return JS_NewBool(ctx, p->link_ok);
    case 0x8B80: return JS_NewBool(ctx, p->deleted);
    case 0x8B85: return JS_NewInt32(ctx, (p->vs ? 1 : 0) + (p->fs ? 1 : 0));
    case 0x8B86: return JS_NewInt32(ctx, p->nunis);
    case 0x8B89: return JS_NewInt32(ctx, p->nattrs);
    }
    return JS_NULL;
}
FN(m_getProgramInfoLog) { C SGLProgram *p = sgl_obj(&g->programs, unwrap(ctx, A(0), OK_PROGRAM)); return p ? JS_NewString(ctx, p->log ? p->log : "") : JS_NULL; }
FN(m_getAttribLocation)
{
    C const char *s = JS_ToCString(ctx, A(1));
    int r = s ? sgl_get_attrib_location(g, unwrap(ctx, A(0), OK_PROGRAM), s) : -1;
    JS_FreeCString(ctx, s);
    return JS_NewInt32(ctx, r);
}
static JSValue active_info(JSContext *ctx, const char *name, int size, int type)
{
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "name", JS_NewString(ctx, name));
    JS_SetPropertyStr(ctx, o, "size", JS_NewInt32(ctx, size));
    JS_SetPropertyStr(ctx, o, "type", JS_NewInt32(ctx, type));
    return o;
}
FN(m_getActiveAttrib)
{
    C SGLProgram *p = sgl_obj(&g->programs, unwrap(ctx, A(0), OK_PROGRAM));
    int i = I(1);
    if (!p || !p->link_ok || i < 0 || i >= p->nattrs) return JS_NULL;
    return active_info(ctx, p->attrs[i].name, 1, p->attrs[i].gltype);
}
FN(m_getActiveUniform)
{
    C SGLProgram *p = sgl_obj(&g->programs, unwrap(ctx, A(0), OK_PROGRAM));
    int i = I(1);
    if (!p || !p->link_ok || i < 0 || i >= p->nunis) return JS_NULL;
    char nm[64];
    if (p->unis[i].type.arr) snprintf(nm, sizeof nm, "%s[0]", p->unis[i].name); else snprintf(nm, sizeof nm, "%s", p->unis[i].name);
    return active_info(ctx, nm, p->unis[i].count, p->unis[i].gltype);
}
FN(m_getUniformLocation)
{
    C int prog = unwrap(ctx, A(0), OK_PROGRAM);
    const char *s = JS_ToCString(ctx, A(1));
    int elem = 0, u = s ? sgl_get_uniform_location(g, prog, s, &elem) : -1;
    JS_FreeCString(ctx, s);
    if (u < 0) return JS_NULL;
    JSValue o = JS_NewObjectProtoClass(ctx, loc_proto, loc_cid);
    GLLoc *l = malloc(sizeof(GLLoc));
    l->prog = prog; l->uidx = u; l->elem = elem;
    JS_SetOpaque(o, l);
    return o;
}
static GLLoc *getloc(JSValueConst v) { return JS_IsObject(v) ? JS_GetOpaque(v, loc_cid) : NULL; }

/* uniform setters: magic encodes (n comps) | vector flag | int flag | matrix */
static JSValue m_uniform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    C GLLoc *l = getloc(A(0));
    if (!l) return JS_UNDEFINED;
    int n = magic & 15, isv = magic & 16, ismat = magic & 64;
    if (ismat) {
        int cnt; float *f = get_floats(ctx, A(2), &cnt);
        if (!f) return JS_UNDEFINED;
        if (JS_ToBool(ctx, A(1))) {   /* transpose (WebGL forbids, but be lenient) */
            for (int m = 0; m + n * n <= cnt; m += n * n) {
                float t[16]; for (int r = 0; r < n; r++) for (int cc = 0; cc < n; cc++) t[cc * n + r] = f[m + r * n + cc];
                memcpy(f + m, t, n * n * sizeof(float));
            }
        }
        sgl_uniform(g, l->prog, l->uidx, l->elem, f, cnt);
        free(f);
        return JS_UNDEFINED;
    }
    if (isv) {
        int cnt; float *f = get_floats(ctx, A(1), &cnt);
        if (!f) return JS_UNDEFINED;
        sgl_uniform(g, l->prog, l->uidx, l->elem, f, cnt);
        free(f);
        return JS_UNDEFINED;
    }
    float v[4];
    for (int i = 0; i < n; i++) v[i] = (float)ad(ctx, A(1 + i));
    sgl_uniform(g, l->prog, l->uidx, l->elem, v, n);
    return JS_UNDEFINED;
}
FN(m_getUniform)
{
    C GLLoc *l = getloc(A(1));
    if (!l) return JS_NULL;
    float v[16]; int n;
    sgl_get_uniform(g, l->prog, l->uidx, l->elem, v, &n);
    if (n == 0) return JS_NULL;
    SGLProgram *p = sgl_obj(&g->programs, l->prog);
    GType t = p->unis[l->uidx].type;
    if (n == 1) return t.kind == GK_BOOL ? JS_NewBool(ctx, v[0] != 0) : (t.kind == GK_FLOAT ? JS_NewFloat64(ctx, v[0]) : JS_NewInt32(ctx, (int)v[0]));
    JSValue a = JS_NewArray(ctx);
    for (int i = 0; i < n; i++) JS_SetPropertyUint32(ctx, a, i, JS_NewFloat64(ctx, v[i]));
    return a;
}

/* ---------------- attributes ---------------- */
FN(m_vertexAttribPointer) { C sgl_vertex_attrib_pointer(g, I(0), I(1), I(2), JS_ToBool(ctx, A(3)), I(4), I(5)); return JS_UNDEFINED; }
FN(m_enableVertexAttribArray) { C sgl_enable_attrib(g, I(0), 1); return JS_UNDEFINED; }
FN(m_disableVertexAttribArray) { C sgl_enable_attrib(g, I(0), 0); return JS_UNDEFINED; }
static JSValue m_vertexAttrib(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    C int n = magic & 15;
    float v[4] = { 0, 0, 0, 1 };
    if (magic & 16) {
        int cnt; float *f = get_floats(ctx, A(1), &cnt);
        if (f) { for (int i = 0; i < n && i < cnt; i++) v[i] = f[i]; free(f); }
    } else for (int i = 0; i < n; i++) v[i] = (float)ad(ctx, A(1 + i));
    sgl_vertex_attrib(g, I(0), v, n);
    return JS_UNDEFINED;
}
FN(m_getVertexAttrib)
{
    C int i = I(0), p = I(1);
    if (i < 0 || i >= SGL_MAX_ATTRIBS) return JS_NULL;
    SGLAttribState *a = &g->attr[i];
    switch (p) {
    case 0x8622: return JS_NewBool(ctx, a->enabled);
    case 0x8623: return JS_NewInt32(ctx, a->size);
    case 0x8624: return JS_NewInt32(ctx, a->stride);
    case 0x8625: return JS_NewInt32(ctx, a->type);
    case 0x886A: return JS_NewBool(ctx, a->normalized);
    case 0x889F: return wrap_obj(ctx, c, OK_BUFFER, a->buffer);
    case 0x8626: { JSValue arr = JS_NewArray(ctx); for (int k = 0; k < 4; k++) JS_SetPropertyUint32(ctx, arr, k, JS_NewFloat64(ctx, a->generic[k])); return arr; }
    }
    return JS_NULL;
}
FN(m_getVertexAttribOffset) { C int i = I(0); return JS_NewInt32(ctx, (i >= 0 && i < SGL_MAX_ATTRIBS) ? g->attr[i].offset : 0); }

/* ---------------- drawing ---------------- */
FN(m_drawArrays) { C sgl_draw_arrays(g, I(0), I(1), I(2)); return JS_UNDEFINED; }
FN(m_drawElements) { C sgl_draw_elements(g, I(0), I(1), I(2), I(3)); return JS_UNDEFINED; }
FN(m_readPixels)
{
    C int x = I(0), y = I(1), w = I(2), h = I(3), type = I(5);
    size_t len; uint8_t *p = get_bytes(ctx, A(6), &len, NULL);
    if (!p || w <= 0 || h <= 0) return JS_UNDEFINED;
    uint8_t *tmp = malloc((size_t)w * h * 4 + 16);
    int pa = g->pack_align; g->pack_align = 1;
    sgl_read_pixels(g, x, y, w, h, tmp);
    g->pack_align = pa;
    if (type == GL_FLOAT) {
        size_t n = len / 4 < (size_t)w * h * 4 ? len / 4 : (size_t)w * h * 4;
        for (size_t i = 0; i < n; i++) { float f = tmp[i] / 255.0f; memcpy(p + i * 4, &f, 4); }
    } else memcpy(p, tmp, len < (size_t)w * h * 4 ? len : (size_t)w * h * 4);
    free(tmp);
    return JS_UNDEFINED;
}

/* ---------------- textures ---------------- */
FN(m_bindTexture) { C int id = unwrap(ctx, A(1), OK_TEXTURE); if (id < 0) { sgl_set_error(g, GL_INVALID_OPERATION); return JS_UNDEFINED; } sgl_bind_texture(g, I(0), id); return JS_UNDEFINED; }
FN(m_texParameteri) { C sgl_tex_parameter(g, I(0), I(1), (int)ad(ctx, A(2))); return JS_UNDEFINED; }
FN(m_getTexParameter)
{
    C int t = I(0), p = I(1);
    SGLTexture *tx = sgl_obj(&g->textures, t == GL_TEXTURE_2D ? g->tex2d[g->active_unit] : g->texcube[g->active_unit]);
    if (!tx) return JS_NULL;
    switch (p) { case GL_TEXTURE_MIN_FILTER: return JS_NewInt32(ctx, tx->minf); case GL_TEXTURE_MAG_FILTER: return JS_NewInt32(ctx, tx->magf);
                 case GL_TEXTURE_WRAP_S: return JS_NewInt32(ctx, tx->wraps); case GL_TEXTURE_WRAP_T: return JS_NewInt32(ctx, tx->wrapt); }
    return JS_NULL;
}
FN(m_generateMipmap) { C sgl_generate_mipmap(g, I(0)); return JS_UNDEFINED; }

/* extract RGBA8 top-down pixels from an image-like source; returns malloc'd copy */
static uint8_t *source_pixels(JSContext *ctx, JSValueConst src, int *w, int *h)
{
    *w = *h = 0;
    if (!JS_IsObject(src)) return NULL;
    /* ImageData {width, height, data} */
    JSValue d = JS_GetPropertyStr(ctx, src, "data");
    if (JS_IsObject(d)) {
        size_t len; uint8_t *p = get_bytes(ctx, d, &len, NULL);
        int ww = ai(ctx, JS_GetPropertyStr(ctx, src, "width")), hh = ai(ctx, JS_GetPropertyStr(ctx, src, "height"));
        if (p && ww > 0 && hh > 0 && len >= (size_t)ww * hh * 4) {
            uint8_t *r = malloc((size_t)ww * hh * 4); memcpy(r, p, (size_t)ww * hh * 4);
            *w = ww; *h = hh; JS_FreeValue(ctx, d); return r;
        }
    }
    JS_FreeValue(ctx, d);
    /* image element: _px ArrayBuffer */
    JSValue px = JS_GetPropertyStr(ctx, src, "_px");
    if (JS_IsObject(px)) {
        size_t len; uint8_t *p = get_bytes(ctx, px, &len, NULL);
        int ww = ai(ctx, JS_GetPropertyStr(ctx, src, "naturalWidth")), hh = ai(ctx, JS_GetPropertyStr(ctx, src, "naturalHeight"));
        JS_FreeValue(ctx, px);
        if (p && ww > 0 && hh > 0 && len >= (size_t)ww * hh * 4) {
            uint8_t *r = malloc((size_t)ww * hh * 4); memcpy(r, p, (size_t)ww * hh * 4);
            *w = ww; *h = hh; return r;
        }
        return NULL;
    }
    JS_FreeValue(ctx, px);
    /* canvas element */
    JSValue cx = JS_GetPropertyStr(ctx, src, "_ctx");
    if (JS_IsObject(cx)) {
        const uint32_t *pp; int ww, hh, flip;
        uint8_t *r = NULL;
        if (canvas_ctx_pixels(ctx, cx, &pp, &ww, &hh, &flip) && pp) {
            r = malloc((size_t)ww * hh * 4);
            for (int y = 0; y < hh; y++) memcpy(r + (size_t)y * ww * 4, pp + (size_t)(flip ? hh - 1 - y : y) * ww, ww * 4);
            *w = ww; *h = hh;
        }
        JS_FreeValue(ctx, cx);
        return r;
    }
    JS_FreeValue(ctx, cx);
    /* empty canvas: blank of its size */
    JSValue tn = JS_GetPropertyStr(ctx, src, "tagName");
    const char *t = JS_ToCString(ctx, tn);
    uint8_t *r = NULL;
    if (t && !strcmp(t, "CANVAS")) {
        int ww = ai(ctx, JS_GetPropertyStr(ctx, src, "width")), hh = ai(ctx, JS_GetPropertyStr(ctx, src, "height"));
        if (ww > 0 && hh > 0) { r = calloc((size_t)ww * hh, 4); *w = ww; *h = hh; }
    }
    JS_FreeCString(ctx, t); JS_FreeValue(ctx, tn);
    return r;
}

FN(m_texImage2D)
{
    C int target = I(0), level = I(1), ifmt = I(2);
    if (argc >= 9) {
        int w = I(3), h = I(4), fmt = I(6), type = I(7);
        size_t len = 0; uint8_t *p = NULL;
        if (!JS_IsNull(A(8)) && !JS_IsUndefined(A(8))) p = get_bytes(ctx, A(8), &len, NULL);
        sgl_tex_image(g, target, level, ifmt, w, h, fmt, type, p);
        return JS_UNDEFINED;
    }
    int fmt = I(3);
    int w, h;
    uint8_t *px = source_pixels(ctx, A(5), &w, &h);
    if (!px) { sgl_tex_image(g, target, level, ifmt, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL); return JS_UNDEFINED; }
    sgl_tex_image_rgba(g, target, level, w, h, px, fmt);
    free(px);
    return JS_UNDEFINED;
}
FN(m_texSubImage2D)
{
    C int target = I(0), level = I(1), x = I(2), y = I(3);
    if (argc >= 9) {
        size_t len; uint8_t *p = get_bytes(ctx, A(8), &len, NULL);
        if (p) sgl_tex_sub_image(g, target, level, x, y, I(4), I(5), I(6), I(7), p);
        return JS_UNDEFINED;
    }
    int w, h;
    uint8_t *px = source_pixels(ctx, A(6), &w, &h);
    if (!px) return JS_UNDEFINED;
    int al = g->unpack_align; g->unpack_align = 1;
    if (g->unpack_flip_y) {   /* tex_sub_image flips rows itself when flip_y; source is top-down */
    }
    sgl_tex_sub_image(g, target, level, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    g->unpack_align = al;
    free(px);
    return JS_UNDEFINED;
}
FN(m_copyTexImage2D) { C sgl_copy_tex_image(g, I(0), I(1), I(3), I(4), I(5), I(6)); return JS_UNDEFINED; }

/* ---------------- framebuffers ---------------- */
FN(m_bindFramebuffer) { C int id = unwrap(ctx, A(1), OK_FRAMEBUFFER); sgl_bind_framebuffer(g, id < 0 ? 0 : id); return JS_UNDEFINED; }
FN(m_bindRenderbuffer) { C int id = unwrap(ctx, A(1), OK_RENDERBUFFER); sgl_bind_renderbuffer(g, id < 0 ? 0 : id); return JS_UNDEFINED; }
FN(m_framebufferTexture2D) { C sgl_framebuffer_texture(g, I(1), I(2), unwrap(ctx, A(3), OK_TEXTURE)); return JS_UNDEFINED; }
FN(m_framebufferRenderbuffer) { C sgl_framebuffer_renderbuffer(g, I(1), unwrap(ctx, A(3), OK_RENDERBUFFER)); return JS_UNDEFINED; }
FN(m_renderbufferStorage) { C sgl_renderbuffer_storage(g, I(1), I(2), I(3)); return JS_UNDEFINED; }
FN(m_checkFramebufferStatus) { C return JS_NewInt32(ctx, sgl_check_framebuffer(g)); }
FN(m_getRenderbufferParameter)
{
    C SGLRenderbuffer *r = sgl_obj(&g->renderbuffers, g->renderbuffer);
    if (!r) return JS_NULL;
    switch (I(1)) { case 0x8D42: return JS_NewInt32(ctx, r->w); case 0x8D43: return JS_NewInt32(ctx, r->h); case 0x8D44: return JS_NewInt32(ctx, r->format); }
    return JS_NewInt32(ctx, 8);
}
FN(m_getFramebufferAttachmentParameter)
{
    C SGLFramebuffer *f = sgl_obj(&g->framebuffers, g->framebuffer);
    if (!f) return JS_NULL;
    int att = I(1), p = I(2);
    if (p == 0x8CD0) {
        if (att == GL_COLOR_ATTACHMENT0) return JS_NewInt32(ctx, f->color_tex ? 0x1702 : f->color_rb ? GL_RENDERBUFFER : 0);
        if (att == GL_DEPTH_ATTACHMENT) return JS_NewInt32(ctx, f->depth_rb ? GL_RENDERBUFFER : 0);
        return JS_NewInt32(ctx, 0);
    }
    if (p == 0x8CD1) {
        if (att == GL_COLOR_ATTACHMENT0) return f->color_tex ? wrap_obj(ctx, c, OK_TEXTURE, f->color_tex) : wrap_obj(ctx, c, OK_RENDERBUFFER, f->color_rb);
        if (att == GL_DEPTH_ATTACHMENT) return wrap_obj(ctx, c, OK_RENDERBUFFER, f->depth_rb);
    }
    return JS_NewInt32(ctx, 0);
}

/* ---------------- queries ---------------- */
static JSValue f32arr(JSContext *ctx, const float *v, int n)
{
    JSValue ab = JS_NewArrayBufferCopy(ctx, (const uint8_t *)v, n * 4);
    JSValue r = new_ta(ctx, ab, JS_TYPED_ARRAY_FLOAT32);
    JS_FreeValue(ctx, ab);
    return r;
}
static JSValue i32arr(JSContext *ctx, const int *v, int n)
{
    JSValue ab = JS_NewArrayBufferCopy(ctx, (const uint8_t *)v, n * 4);
    JSValue r = new_ta(ctx, ab, JS_TYPED_ARRAY_INT32);
    JS_FreeValue(ctx, ab);
    return r;
}
FN(m_getParameter)
{
    C int p = I(0);
    switch (p) {
    case 0x1F00: return JS_NewString(ctx, "nspGL");
    case 0x1F01: return JS_NewString(ctx, "nspGL software rasterizer (TI-Nspire CX)");
    case 0x1F02: return JS_NewString(ctx, "WebGL 1.0 (nspGL)");
    case 0x8B8C: return JS_NewString(ctx, "WebGL GLSL ES 1.0 (nspGL)");
    case 0x9245: return JS_NewString(ctx, "nspGL");
    case 0x9246: return JS_NewString(ctx, "nspGL software");
    case 0x0D33: case 0x851C: case 0x84E8: return JS_NewInt32(ctx, 2048);
    case 0x8869: return JS_NewInt32(ctx, SGL_MAX_ATTRIBS);
    case 0x8DFB: return JS_NewInt32(ctx, 256);
    case 0x8DFC: return JS_NewInt32(ctx, 16);
    case 0x8DFD: return JS_NewInt32(ctx, 256);
    case 0x8B4D: case 0x8872: return JS_NewInt32(ctx, SGL_MAX_UNITS);
    case 0x8B4C: return JS_NewInt32(ctx, 4);
    case 0x0D3A: { int v[2] = { 4096, 4096 }; return i32arr(ctx, v, 2); }
    case 0x846D: { float v[2] = { 1, 128 }; return f32arr(ctx, v, 2); }
    case 0x846E: { float v[2] = { 1, 8 }; return f32arr(ctx, v, 2); }
    case 0x0BA2: return i32arr(ctx, g->viewport, 4);
    case 0x0C10: return i32arr(ctx, g->scissor, 4);
    case 0x0C22: return f32arr(ctx, g->clear_color, 4);
    case 0x8005: return f32arr(ctx, g->blend_color, 4);
    case 0x0B70: return f32arr(ctx, g->depth_range, 2);
    case 0x0C23: { JSValue a = JS_NewArray(ctx); for (int i = 0; i < 4; i++) JS_SetPropertyUint32(ctx, a, i, JS_NewBool(ctx, g->color_mask[i])); return a; }
    case 0x0B73: return JS_NewFloat64(ctx, g->clear_depth);
    case 0x0B74: return JS_NewInt32(ctx, g->depth_func);
    case 0x0B72: return JS_NewBool(ctx, g->depth_mask);
    case 0x0B45: return JS_NewInt32(ctx, g->cull_face);
    case 0x0B46: return JS_NewInt32(ctx, g->front_face);
    case 0x0B21: return JS_NewFloat64(ctx, g->line_width);
    case 0x80C9: return JS_NewInt32(ctx, g->bsrc_rgb);
    case 0x80C8: return JS_NewInt32(ctx, g->bdst_rgb);
    case 0x80CB: return JS_NewInt32(ctx, g->bsrc_a);
    case 0x80CA: return JS_NewInt32(ctx, g->bdst_a);
    case 0x8009: return JS_NewInt32(ctx, g->beq_rgb);
    case 0x883D: return JS_NewInt32(ctx, g->beq_a);
    case 0x84E0: return JS_NewInt32(ctx, GL_TEXTURE0 + g->active_unit);
    case 0x8B8D: return wrap_obj(ctx, c, OK_PROGRAM, g->program);
    case 0x8894: return wrap_obj(ctx, c, OK_BUFFER, g->array_buffer);
    case 0x8895: return wrap_obj(ctx, c, OK_BUFFER, g->element_buffer);
    case 0x8069: return wrap_obj(ctx, c, OK_TEXTURE, g->tex2d[g->active_unit]);
    case 0x8514: return wrap_obj(ctx, c, OK_TEXTURE, g->texcube[g->active_unit]);
    case 0x8CA6: return wrap_obj(ctx, c, OK_FRAMEBUFFER, g->framebuffer);
    case 0x8CA7: return wrap_obj(ctx, c, OK_RENDERBUFFER, g->renderbuffer);
    case 0x0CF5: return JS_NewInt32(ctx, g->unpack_align);
    case 0x0D05: return JS_NewInt32(ctx, g->pack_align);
    case 0x9240: return JS_NewBool(ctx, g->unpack_flip_y);
    case 0x9241: return JS_NewBool(ctx, g->unpack_premul);
    case 0x0D50: return JS_NewInt32(ctx, 4);
    case 0x0D52: case 0x0D53: case 0x0D54: return JS_NewInt32(ctx, 8);
    case 0x0D55: return JS_NewInt32(ctx, c->alpha ? 8 : 0);
    case 0x0D56: return JS_NewInt32(ctx, c->depth ? 16 : 0);
    case 0x0D57: return JS_NewInt32(ctx, 0);
    case 0x80A8: case 0x80A9: return JS_NewInt32(ctx, 0);
    case 0x86A3: return i32arr(ctx, NULL, 0);
    case 0x8B9A: return JS_NewInt32(ctx, GL_UNSIGNED_BYTE);
    case 0x8B9B: return JS_NewInt32(ctx, GL_RGBA);
    case 0x0B92: case 0x8800: return JS_NewInt32(ctx, g->stencil_func);
    case 0x0B97: case 0x8CA3: return JS_NewInt32(ctx, g->stencil_ref);
    case 0x0B93: case 0x8CA4: case 0x0B98: case 0x8CA5: return JS_NewInt32(ctx, 0xFF);
    case 0x0B94: case 0x0B95: case 0x0B96: case 0x8801: case 0x8802: case 0x8803: return JS_NewInt32(ctx, 0x1E00);
    case 0x0B91: return JS_NewInt32(ctx, 0);
    case 0x2A00: return JS_NewFloat64(ctx, g->po_units);
    case 0x8038: return JS_NewFloat64(ctx, g->po_factor);
    case 0x8192: return JS_NewInt32(ctx, 0x1100);
    case 0x80AA: return JS_NewFloat64(ctx, 1);
    case 0x80AB: return JS_FALSE;
    case 0x9243: return JS_NewInt32(ctx, 0x9244);
    case GL_CULL_FACE: case GL_BLEND: case GL_DEPTH_TEST: case GL_SCISSOR_TEST: case GL_STENCIL_TEST: case GL_DITHER: case GL_POLYGON_OFFSET_FILL:
        return JS_NewBool(ctx, sgl_is_enabled(g, p));
    }
    sgl_set_error(g, GL_INVALID_ENUM);
    return JS_NULL;
}
FN(m_getContextAttributes)
{
    C JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "alpha", JS_NewBool(ctx, c->alpha));
    JS_SetPropertyStr(ctx, o, "depth", JS_NewBool(ctx, c->depth));
    JS_SetPropertyStr(ctx, o, "stencil", JS_FALSE);
    JS_SetPropertyStr(ctx, o, "antialias", JS_FALSE);
    JS_SetPropertyStr(ctx, o, "premultipliedAlpha", JS_NewBool(ctx, c->premul));
    JS_SetPropertyStr(ctx, o, "preserveDrawingBuffer", JS_NewBool(ctx, c->preserve));
    JS_SetPropertyStr(ctx, o, "failIfMajorPerformanceCaveat", JS_FALSE);
    JS_SetPropertyStr(ctx, o, "powerPreference", JS_NewString(ctx, "default"));
    return o;
}
static const char *exts[] = { "OES_element_index_uint", "OES_standard_derivatives", "OES_texture_float", "OES_texture_float_linear",
                              "OES_texture_half_float", "OES_texture_half_float_linear", "WEBGL_lose_context", "WEBGL_debug_renderer_info", NULL };
FN(m_getSupportedExtensions)
{
    (void)this_val; (void)argc; (void)argv;
    JSValue a = JS_NewArray(ctx);
    for (int i = 0; exts[i]; i++) JS_SetPropertyUint32(ctx, a, i, JS_NewString(ctx, exts[i]));
    return a;
}
FN(m_getExtension)
{
    (void)this_val;
    const char *s = JS_ToCString(ctx, A(0));
    JSValue r = JS_NULL;
    if (s) for (int i = 0; exts[i]; i++) if (!strcasecmp(s, exts[i])) {
        r = JS_NewObject(ctx);
        if (!strcmp(exts[i], "OES_standard_derivatives")) JS_SetPropertyStr(ctx, r, "FRAGMENT_SHADER_DERIVATIVE_HINT_OES", JS_NewInt32(ctx, 0x8B8B));
        if (!strcmp(exts[i], "OES_texture_half_float")) JS_SetPropertyStr(ctx, r, "HALF_FLOAT_OES", JS_NewInt32(ctx, 0x8D61));
        if (!strcmp(exts[i], "WEBGL_debug_renderer_info")) {
            JS_SetPropertyStr(ctx, r, "UNMASKED_VENDOR_WEBGL", JS_NewInt32(ctx, 0x9245));
            JS_SetPropertyStr(ctx, r, "UNMASKED_RENDERER_WEBGL", JS_NewInt32(ctx, 0x9246));
        }
        if (!strcmp(exts[i], "WEBGL_lose_context")) {
            JS_SetPropertyStr(ctx, r, "loseContext", JS_NewCFunction(ctx, m_noop, "loseContext", 0));
            JS_SetPropertyStr(ctx, r, "restoreContext", JS_NewCFunction(ctx, m_noop, "restoreContext", 0));
        }
        break;
    }
    JS_FreeCString(ctx, s);
    return r;
}
FN(m_finish) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
FN(m_resize)
{
    C int w = I(0), h = I(1);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w == c->vw && h == c->vh) return JS_UNDEFINED;
    int rw, rh; real_size(w, h, &rw, &rh);
    c->vw = w; c->vh = h;
    sgl_resize(g, w, h, rw, rh);
    /* WebGL resets viewport only at creation; keep app's viewport */
    return JS_UNDEFINED;
}
static JSValue get_dbw(JSContext *ctx, JSValueConst this_val) { GLCtx *c = getctx(ctx, this_val); return c ? JS_NewInt32(ctx, c->vw) : JS_EXCEPTION; }
static JSValue get_dbh(JSContext *ctx, JSValueConst this_val) { GLCtx *c = getctx(ctx, this_val); return c ? JS_NewInt32(ctx, c->vh) : JS_EXCEPTION; }

#define M(n, f, a) JS_CFUNC_DEF(n, a, f)
#define U(n, mag, a) JS_CFUNC_MAGIC_DEF(n, a, m_uniform, mag)
#define VA(n, mag, a) JS_CFUNC_MAGIC_DEF(n, a, m_vertexAttrib, mag)
static const JSCFunctionListEntry gl_funcs[] = {
    M("viewport", m_viewport, 4), M("scissor", m_scissor, 4), M("clearColor", m_clearColor, 4), M("clearDepth", m_clearDepth, 1),
    M("clearStencil", m_clearStencil, 1), M("clear", m_clear, 1), M("enable", m_enable, 1), M("disable", m_disable, 1),
    M("isEnabled", m_isEnabled, 1), M("depthFunc", m_depthFunc, 1), M("depthMask", m_depthMask, 1), M("depthRange", m_depthRange, 2),
    M("cullFace", m_cullFace, 1), M("frontFace", m_frontFace, 1), M("blendFunc", m_blendFunc, 2), M("blendFuncSeparate", m_blendFuncSeparate, 4),
    M("blendEquation", m_blendEquation, 1), M("blendEquationSeparate", m_blendEquationSeparate, 2), M("blendColor", m_blendColor, 4),
    M("colorMask", m_colorMask, 4), M("lineWidth", m_lineWidth, 1), M("polygonOffset", m_polygonOffset, 2),
    M("stencilFunc", m_stencilFunc, 3), M("stencilFuncSeparate", m_noop, 4), M("stencilMask", m_stencilMask, 1),
    M("stencilMaskSeparate", m_noop, 2), M("stencilOp", m_noop, 3), M("stencilOpSeparate", m_noop, 4),
    M("hint", m_noop, 2), M("sampleCoverage", m_noop, 2), M("pixelStorei", m_pixelStorei, 2), M("getError", m_getError, 0),
    M("isContextLost", m_isContextLost, 0), M("activeTexture", m_activeTexture, 1), M("flush", m_finish, 0), M("finish", m_finish, 0),
    M("createBuffer", m_createBuffer, 0), M("createTexture", m_createTexture, 0), M("createProgram", m_createProgram, 0),
    M("createFramebuffer", m_createFramebuffer, 0), M("createRenderbuffer", m_createRenderbuffer, 0), M("createShader", m_createShader, 1),
    M("deleteBuffer", m_deleteBuffer, 1), M("deleteTexture", m_deleteTexture, 1), M("deleteProgram", m_deleteProgram, 1),
    M("deleteShader", m_deleteShader, 1), M("deleteFramebuffer", m_deleteFramebuffer, 1), M("deleteRenderbuffer", m_deleteRenderbuffer, 1),
    M("isBuffer", m_isBuffer, 1), M("isTexture", m_isTexture, 1), M("isProgram", m_isProgram, 1), M("isShader", m_isShader, 1),
    M("isFramebuffer", m_isFramebuffer, 1), M("isRenderbuffer", m_isRenderbuffer, 1),
    M("bindBuffer", m_bindBuffer, 2), M("bufferData", m_bufferData, 3), M("bufferSubData", m_bufferSubData, 3),
    M("getBufferParameter", m_getBufferParameter, 2),
    M("shaderSource", m_shaderSource, 2), M("getShaderSource", m_getShaderSource, 1), M("compileShader", m_compileShader, 1),
    M("getShaderParameter", m_getShaderParameter, 2), M("getShaderInfoLog", m_getShaderInfoLog, 1),
    M("getShaderPrecisionFormat", m_getShaderPrecisionFormat, 2), M("attachShader", m_attachShader, 2), M("detachShader", m_detachShader, 2),
    M("getAttachedShaders", m_getAttachedShaders, 1), M("bindAttribLocation", m_bindAttribLocation, 3), M("linkProgram", m_linkProgram, 1),
    M("useProgram", m_useProgram, 1), M("validateProgram", m_validateProgram, 1), M("getProgramParameter", m_getProgramParameter, 2),
    M("getProgramInfoLog", m_getProgramInfoLog, 1), M("getAttribLocation", m_getAttribLocation, 2), M("getActiveAttrib", m_getActiveAttrib, 2),
    M("getActiveUniform", m_getActiveUniform, 2), M("getUniformLocation", m_getUniformLocation, 2), M("getUniform", m_getUniform, 2),
    U("uniform1f", 1, 2), U("uniform2f", 2, 3), U("uniform3f", 3, 4), U("uniform4f", 4, 5),
    U("uniform1i", 1, 2), U("uniform2i", 2, 3), U("uniform3i", 3, 4), U("uniform4i", 4, 5),
    U("uniform1fv", 17, 2), U("uniform2fv", 18, 2), U("uniform3fv", 19, 2), U("uniform4fv", 20, 2),
    U("uniform1iv", 17, 2), U("uniform2iv", 18, 2), U("uniform3iv", 19, 2), U("uniform4iv", 20, 2),
    U("uniformMatrix2fv", 64 | 2, 3), U("uniformMatrix3fv", 64 | 3, 3), U("uniformMatrix4fv", 64 | 4, 3),
    M("vertexAttribPointer", m_vertexAttribPointer, 6), M("enableVertexAttribArray", m_enableVertexAttribArray, 1),
    M("disableVertexAttribArray", m_disableVertexAttribArray, 1),
    VA("vertexAttrib1f", 1, 2), VA("vertexAttrib2f", 2, 3), VA("vertexAttrib3f", 3, 4), VA("vertexAttrib4f", 4, 5),
    VA("vertexAttrib1fv", 17, 2), VA("vertexAttrib2fv", 18, 2), VA("vertexAttrib3fv", 19, 2), VA("vertexAttrib4fv", 20, 2),
    M("getVertexAttrib", m_getVertexAttrib, 2), M("getVertexAttribOffset", m_getVertexAttribOffset, 2),
    M("drawArrays", m_drawArrays, 3), M("drawElements", m_drawElements, 4), M("readPixels", m_readPixels, 7),
    M("bindTexture", m_bindTexture, 2), M("texParameteri", m_texParameteri, 3), M("texParameterf", m_texParameteri, 3),
    M("getTexParameter", m_getTexParameter, 2), M("generateMipmap", m_generateMipmap, 1), M("texImage2D", m_texImage2D, 6),
    M("texSubImage2D", m_texSubImage2D, 7), M("copyTexImage2D", m_copyTexImage2D, 8), M("copyTexSubImage2D", m_noop, 8),
    M("compressedTexImage2D", m_noop, 7), M("compressedTexSubImage2D", m_noop, 8),
    M("bindFramebuffer", m_bindFramebuffer, 2), M("bindRenderbuffer", m_bindRenderbuffer, 2),
    M("framebufferTexture2D", m_framebufferTexture2D, 5), M("framebufferRenderbuffer", m_framebufferRenderbuffer, 4),
    M("renderbufferStorage", m_renderbufferStorage, 4), M("checkFramebufferStatus", m_checkFramebufferStatus, 1),
    M("getRenderbufferParameter", m_getRenderbufferParameter, 2), M("getFramebufferAttachmentParameter", m_getFramebufferAttachmentParameter, 3),
    M("getParameter", m_getParameter, 1), M("getContextAttributes", m_getContextAttributes, 0),
    M("getSupportedExtensions", m_getSupportedExtensions, 0), M("getExtension", m_getExtension, 1),
    M("__resize", m_resize, 2),
    JS_CGETSET_DEF("drawingBufferWidth", get_dbw, NULL), JS_CGETSET_DEF("drawingBufferHeight", get_dbh, NULL),
};

/* WebGL constants */
static const struct { const char *n; int v; } consts[] = {
#include "webgl_consts.inc"
    { NULL, 0 } };

static void gl_finalizer(JSRuntime *rt, JSValue val)
{
    GLCtx *c = JS_GetOpaque(val, gl_cid);
    if (!c) return;
    for (int k = 0; k < OK_COUNT; k++) {
        for (int i = 0; i < c->nobjs[k]; i++) JS_FreeValueRT(rt, c->objs[k][i]);
        free(c->objs[k]);
    }
    sgl_destroy(c->g);
    free(c);
}
static void gl_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    GLCtx *c = JS_GetOpaque(val, gl_cid);
    if (!c) return;
    for (int k = 0; k < OK_COUNT; k++) for (int i = 0; i < c->nobjs[k]; i++) JS_MarkValue(rt, c->objs[k][i], mark_func);
}
static void obj_finalizer(JSRuntime *rt, JSValue val) { (void)rt; free(JS_GetOpaque(val, obj_cid)); }
static void loc_finalizer(JSRuntime *rt, JSValue val) { (void)rt; free(JS_GetOpaque(val, loc_cid)); }
static JSClassDef gl_class = { "WebGLRenderingContext", .finalizer = gl_finalizer, .gc_mark = gl_mark };
static JSClassDef obj_class = { "WebGLObject", .finalizer = obj_finalizer };
static JSClassDef loc_class = { "WebGLUniformLocation", .finalizer = loc_finalizer };

static JSValue illegal_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_ThrowTypeError(ctx, "Illegal constructor");
}

void webgl_init(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&gl_cid); JS_NewClassID(&obj_cid); JS_NewClassID(&loc_cid);
    JS_NewClass(rt, gl_cid, &gl_class);
    JS_NewClass(rt, obj_cid, &obj_class);
    JS_NewClass(rt, loc_cid, &loc_class);
    JSValue global = JS_GetGlobalObject(ctx);
    gl_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, gl_proto, gl_funcs, sizeof gl_funcs / sizeof gl_funcs[0]);
    JSValue ctor = JS_NewCFunction2(ctx, illegal_ctor, "WebGLRenderingContext", 0, JS_CFUNC_constructor, 0);
    for (int i = 0; consts[i].n; i++) {
        JS_DefinePropertyValueStr(ctx, gl_proto, consts[i].n, JS_NewInt32(ctx, consts[i].v), JS_PROP_ENUMERABLE);
        JS_DefinePropertyValueStr(ctx, ctor, consts[i].n, JS_NewInt32(ctx, consts[i].v), JS_PROP_ENUMERABLE);
    }
    JS_SetConstructor(ctx, ctor, gl_proto);
    JS_SetClassProto(ctx, gl_cid, JS_DupValue(ctx, gl_proto));
    JS_SetPropertyStr(ctx, global, "WebGLRenderingContext", ctor);
    for (int k = 0; k < OK_COUNT; k++) {
        obj_proto[k] = JS_NewObject(ctx);
        JSValue oc = JS_NewCFunction2(ctx, illegal_ctor, ok_names[k], 0, JS_CFUNC_constructor, 0);
        JS_SetConstructor(ctx, oc, obj_proto[k]);
        JS_SetPropertyStr(ctx, global, ok_names[k], oc);
    }
    loc_proto = JS_NewObject(ctx);
    JSValue lc = JS_NewCFunction2(ctx, illegal_ctor, "WebGLUniformLocation", 0, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, lc, loc_proto);
    JS_SetPropertyStr(ctx, global, "WebGLUniformLocation", lc);
    JS_FreeValue(ctx, global);
}

void webgl_free_protos(JSContext *ctx)
{
    JS_FreeValue(ctx, gl_proto);
    for (int k = 0; k < OK_COUNT; k++) JS_FreeValue(ctx, obj_proto[k]);
    JS_FreeValue(ctx, loc_proto);
}

JSValue webgl_create(JSContext *ctx, JSValueConst canvas, JSValueConst attrs)
{
    GLCtx *c = calloc(1, sizeof(GLCtx));
    int vw = ai(ctx, JS_GetPropertyStr(ctx, canvas, "width")), vh = ai(ctx, JS_GetPropertyStr(ctx, canvas, "height"));
    if (vw < 1) vw = 1;
    if (vh < 1) vh = 1;
    c->alpha = 1; c->depth = 1; c->premul = 1;
    if (JS_IsObject(attrs)) {
        JSValue v;
        v = JS_GetPropertyStr(ctx, attrs, "alpha"); if (!JS_IsUndefined(v)) c->alpha = JS_ToBool(ctx, v); JS_FreeValue(ctx, v);
        v = JS_GetPropertyStr(ctx, attrs, "depth"); if (!JS_IsUndefined(v)) c->depth = JS_ToBool(ctx, v); JS_FreeValue(ctx, v);
        v = JS_GetPropertyStr(ctx, attrs, "preserveDrawingBuffer"); c->preserve = JS_ToBool(ctx, v); JS_FreeValue(ctx, v);
    }
    int rw, rh; real_size(vw, vh, &rw, &rh);
    c->g = sgl_create(vw, vh, rw, rh, c->depth, c->alpha);
    c->vw = vw; c->vh = vh;
    JSValue o = JS_NewObjectProtoClass(ctx, gl_proto, gl_cid);
    JS_SetOpaque(o, c);
    JS_SetPropertyStr(ctx, o, "canvas", JS_DupValue(ctx, canvas));
    return o;
}

int webgl_pixels(JSContext *ctx, JSValueConst ctxobj, const uint32_t **px, int *w, int *h, int *alpha)
{
    (void)ctx;
    GLCtx *c = JS_GetOpaque(ctxobj, gl_cid);
    if (!c) return 0;
    *px = c->g->color; *w = c->g->w; *h = c->g->h; *alpha = c->alpha;
    return 1;
}
SGL *webgl_sgl(JSValueConst ctxobj) { GLCtx *c = JS_GetOpaque(ctxobj, gl_cid); return c ? c->g : NULL; }
