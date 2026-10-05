/* SGL: software WebGL 1.0 rasterizer */
#include "sgl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define MAXV 64          /* max varying floats */

/* ---------------- object tables ---------------- */
int sgl_create_obj(SGL *g, SGLObjTab *t, void *p)
{
    (void)g;
    for (int i = 0; i < t->n; i++) if (!t->v[i]) { t->v[i] = p; return i + 1; }
    if (t->n == t->cap) { t->cap = t->cap ? t->cap * 2 : 16; t->v = realloc(t->v, t->cap * sizeof(void *)); }
    t->v[t->n++] = p;
    return t->n;
}
void *sgl_obj(SGLObjTab *t, int id) { return (id > 0 && id <= t->n) ? t->v[id - 1] : NULL; }
static void obj_clear(SGLObjTab *t, int id) { if (id > 0 && id <= t->n) t->v[id - 1] = NULL; }

void sgl_set_error(SGL *g, int e) { if (!g->error) g->error = e; }

/* ---------------- context ---------------- */
static void alloc_default(SGL *g)
{
    free(g->color); free(g->depth);
    g->color = calloc((size_t)g->w * g->h, 4);
    g->depth = g->has_depth ? malloc((size_t)g->w * g->h * sizeof(float)) : NULL;
    if (g->depth) for (int i = 0; i < g->w * g->h; i++) g->depth[i] = 1.0f;
    g->sx = (float)g->w / g->vw; g->sy = (float)g->h / g->vh;
}

SGL *sgl_create(int vw, int vh, int rw, int rh, int depth, int alpha)
{
    SGL *g = calloc(1, sizeof(SGL));
    g->vw = vw > 0 ? vw : 1; g->vh = vh > 0 ? vh : 1;
    g->w = rw > 0 ? rw : 1; g->h = rh > 0 ? rh : 1;
    g->has_depth = depth; g->has_alpha = alpha;
    alloc_default(g);
    g->clear_depth = 1.0f;
    g->viewport[2] = vw; g->viewport[3] = vh;
    g->scissor[2] = vw; g->scissor[3] = vh;
    g->depth_func = GL_LESS; g->depth_mask = 1;
    g->cull_face = GL_BACK; g->front_face = GL_CCW;
    g->bsrc_rgb = g->bsrc_a = GL_ONE; g->bdst_rgb = g->bdst_a = GL_ZERO;
    g->beq_rgb = g->beq_a = GL_FUNC_ADD;
    for (int i = 0; i < 4; i++) g->color_mask[i] = 1;
    g->depth_range[0] = 0; g->depth_range[1] = 1;
    g->line_width = 1;
    g->unpack_align = 4; g->pack_align = 4;
    g->dither = 1;
    g->stencil_func = GL_ALWAYS; g->stencil_mask = 0xFF; g->stencil_wmask = 0xFF;
    for (int i = 0; i < SGL_MAX_ATTRIBS; i++) { g->attr[i].generic[3] = 1; g->attr[i].size = 4; g->attr[i].type = GL_FLOAT; }
    return g;
}

void sgl_resize(SGL *g, int vw, int vh, int rw, int rh)
{
    g->vw = vw > 0 ? vw : 1; g->vh = vh > 0 ? vh : 1;
    g->w = rw > 0 ? rw : 1; g->h = rh > 0 ? rh : 1;
    alloc_default(g);
}

static void free_tex(SGLTexture *t) { if (!t) return; free(t->px); for (int i = 0; i < 6; i++) free(t->face[i]); free(t); }
static void free_prog_link(SGLProgram *p)
{
    glsl_free(p->v); glsl_free(p->f); p->v = p->f = NULL;
    free(p->vmem); free(p->fmem); p->vmem = p->fmem = NULL;
    free(p->attrs); free(p->unis); p->attrs = NULL; p->unis = NULL; p->nattrs = p->nunis = 0;
    free(p->vary_vs); free(p->vary_fs); p->vary_vs = p->vary_fs = NULL; p->nvary = 0;
}

void sgl_destroy(SGL *g)
{
    if (!g) return;
    for (int i = 0; i < g->buffers.n; i++) { SGLBuffer *b = g->buffers.v[i]; if (b) { free(b->data); free(b); } }
    for (int i = 0; i < g->textures.n; i++) free_tex(g->textures.v[i]);
    for (int i = 0; i < g->shaders.n; i++) { SGLShader *s = g->shaders.v[i]; if (s) { free(s->src); free(s->log); free(s); } }
    for (int i = 0; i < g->programs.n; i++) {
        SGLProgram *p = g->programs.v[i];
        if (p) { free_prog_link(p); free(p->log); for (int k = 0; k < SGL_MAX_ATTRIBS; k++) free(p->bind_names[k]); free(p); }
    }
    for (int i = 0; i < g->framebuffers.n; i++) free(g->framebuffers.v[i]);
    for (int i = 0; i < g->renderbuffers.n; i++) { SGLRenderbuffer *r = g->renderbuffers.v[i]; if (r) { free(r->depth); free(r->color); free(r); } }
    free(g->buffers.v); free(g->textures.v); free(g->shaders.v); free(g->programs.v); free(g->framebuffers.v); free(g->renderbuffers.v);
    free(g->color); free(g->depth); free(g->vcache); free(g->vdone);
    free(g);
}

/* ---------------- buffers ---------------- */
int sgl_create_buffer(SGL *g) { return sgl_create_obj(g, &g->buffers, calloc(1, sizeof(SGLBuffer))); }
void sgl_delete_buffer(SGL *g, int id)
{
    SGLBuffer *b = sgl_obj(&g->buffers, id);
    if (!b) return;
    free(b->data); free(b); obj_clear(&g->buffers, id);
    if (g->array_buffer == id) g->array_buffer = 0;
    if (g->element_buffer == id) g->element_buffer = 0;
    for (int i = 0; i < SGL_MAX_ATTRIBS; i++) if (g->attr[i].buffer == id) g->attr[i].buffer = 0;
}
void sgl_bind_buffer(SGL *g, int target, int id)
{
    if (target == GL_ARRAY_BUFFER) g->array_buffer = id;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) g->element_buffer = id;
    else sgl_set_error(g, GL_INVALID_ENUM);
}
static SGLBuffer *bound_buf(SGL *g, int target)
{
    int id = target == GL_ARRAY_BUFFER ? g->array_buffer : target == GL_ELEMENT_ARRAY_BUFFER ? g->element_buffer : -1;
    if (id < 0) { sgl_set_error(g, GL_INVALID_ENUM); return NULL; }
    SGLBuffer *b = sgl_obj(&g->buffers, id);
    if (!b) sgl_set_error(g, GL_INVALID_OPERATION);
    return b;
}
void sgl_buffer_data(SGL *g, int target, const void *data, int size, int usage)
{
    SGLBuffer *b = bound_buf(g, target);
    if (!b) return;
    if (size < 0) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    free(b->data);
    b->data = malloc(size ? size : 1);
    if (data) memcpy(b->data, data, size); else memset(b->data, 0, size);
    b->size = size; b->usage = usage;
}
void sgl_buffer_sub_data(SGL *g, int target, int offset, const void *data, int size)
{
    SGLBuffer *b = bound_buf(g, target);
    if (!b) return;
    if (offset < 0 || offset + size > b->size) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    memcpy(b->data + offset, data, size);
}

/* ---------------- textures ---------------- */
int sgl_create_texture(SGL *g)
{
    SGLTexture *t = calloc(1, sizeof(SGLTexture));
    t->minf = GL_NEAREST_MIPMAP_LINEAR; t->magf = GL_LINEAR; t->wraps = t->wrapt = GL_REPEAT;
    return sgl_create_obj(g, &g->textures, t);
}
void sgl_delete_texture(SGL *g, int id)
{
    SGLTexture *t = sgl_obj(&g->textures, id);
    if (!t) return;
    free_tex(t); obj_clear(&g->textures, id);
    for (int i = 0; i < SGL_MAX_UNITS; i++) { if (g->tex2d[i] == id) g->tex2d[i] = 0; if (g->texcube[i] == id) g->texcube[i] = 0; }
}
void sgl_bind_texture(SGL *g, int target, int id)
{
    SGLTexture *t = sgl_obj(&g->textures, id);
    if (t) {
        if (t->target && t->target != target) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
        t->target = target;
    }
    if (target == GL_TEXTURE_2D) g->tex2d[g->active_unit] = id;
    else if (target == GL_TEXTURE_CUBE_MAP) g->texcube[g->active_unit] = id;
    else sgl_set_error(g, GL_INVALID_ENUM);
}
static SGLTexture *bound_tex(SGL *g, int target)
{
    int id;
    if (target == GL_TEXTURE_2D) id = g->tex2d[g->active_unit];
    else if (target == GL_TEXTURE_CUBE_MAP || (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target < GL_TEXTURE_CUBE_MAP_POSITIVE_X + 6))
        id = g->texcube[g->active_unit];
    else { sgl_set_error(g, GL_INVALID_ENUM); return NULL; }
    SGLTexture *t = sgl_obj(&g->textures, id);
    if (!t) sgl_set_error(g, GL_INVALID_OPERATION);
    return t;
}
void sgl_tex_parameter(SGL *g, int target, int pname, int v)
{
    SGLTexture *t = bound_tex(g, target);
    if (!t) return;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER: t->minf = v; break;
    case GL_TEXTURE_MAG_FILTER: t->magf = v; break;
    case GL_TEXTURE_WRAP_S: t->wraps = v; break;
    case GL_TEXTURE_WRAP_T: t->wrapt = v; break;
    default: sgl_set_error(g, GL_INVALID_ENUM);
    }
}

static inline uint32_t pack(int r, int gg, int b, int a) { return (uint32_t)r | ((uint32_t)gg << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24); }

static int fmt_bpp(int fmt, int type)
{
    if (type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1) return 2;
    int ch = fmt == GL_RGBA ? 4 : fmt == GL_RGB ? 3 : fmt == GL_LUMINANCE_ALPHA ? 2 : 1;
    int sz = type == GL_FLOAT ? 4 : (type == GL_HALF_FLOAT_OES || type == GL_UNSIGNED_SHORT) ? 2 : 1;
    return ch * sz;
}
static float half2f(uint16_t h)
{
    int s = h >> 15, e = (h >> 10) & 31, m = h & 1023;
    float v = e == 0 ? m * (1.0f / 16777216.0f) : e == 31 ? 65504.0f : ldexpf(1.0f + m / 1024.0f, e - 15);
    return s ? -v : v;
}
static inline int cl8f(float f) { int v = (int)(f * 255.0f + 0.5f); return v < 0 ? 0 : v > 255 ? 255 : v; }

/* convert one source pixel to RGBA8 */
static uint32_t conv_px(const uint8_t *s, int fmt, int type)
{
    int c[4] = { 0, 0, 0, 255 };
    if (type == GL_UNSIGNED_SHORT_5_6_5) {
        uint16_t v = s[0] | (s[1] << 8);
        return pack(((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31, 255);
    }
    if (type == GL_UNSIGNED_SHORT_4_4_4_4) {
        uint16_t v = s[0] | (s[1] << 8);
        return pack(((v >> 12) & 15) * 17, ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17);
    }
    if (type == GL_UNSIGNED_SHORT_5_5_5_1) {
        uint16_t v = s[0] | (s[1] << 8);
        return pack(((v >> 11) & 31) * 255 / 31, ((v >> 6) & 31) * 255 / 31, ((v >> 1) & 31) * 255 / 31, (v & 1) * 255);
    }
    int ch = fmt == GL_RGBA ? 4 : fmt == GL_RGB ? 3 : fmt == GL_LUMINANCE_ALPHA ? 2 : 1;
    int v[4];
    for (int i = 0; i < ch; i++) {
        if (type == GL_FLOAT) { float f; memcpy(&f, s + i * 4, 4); v[i] = cl8f(f); }
        else if (type == GL_HALF_FLOAT_OES) { uint16_t h = s[i * 2] | (s[i * 2 + 1] << 8); v[i] = cl8f(half2f(h)); }
        else v[i] = s[i];
    }
    switch (fmt) {
    case GL_RGBA: c[0] = v[0]; c[1] = v[1]; c[2] = v[2]; c[3] = v[3]; break;
    case GL_RGB: c[0] = v[0]; c[1] = v[1]; c[2] = v[2]; break;
    case GL_LUMINANCE: c[0] = c[1] = c[2] = v[0]; break;
    case GL_LUMINANCE_ALPHA: c[0] = c[1] = c[2] = v[0]; c[3] = v[1]; break;
    case GL_ALPHA: c[0] = c[1] = c[2] = 0; c[3] = v[0]; break;
    default: c[0] = v[0]; break;
    }
    return pack(c[0], c[1], c[2], c[3]);
}
static uint32_t premul(uint32_t p)
{
    int a = p >> 24;
    return pack((p & 255) * a / 255, ((p >> 8) & 255) * a / 255, ((p >> 16) & 255) * a / 255, a);
}

static uint32_t **tex_level_ptr(SGLTexture *t, int target)
{
    if (target == GL_TEXTURE_2D) return &t->px;
    return &t->face[target - GL_TEXTURE_CUBE_MAP_POSITIVE_X];
}

void sgl_tex_image(SGL *g, int target, int level, int ifmt, int w, int h, int fmt, int type, const void *data)
{
    (void)ifmt;
    SGLTexture *t = bound_tex(g, target);
    if (!t) return;
    if (level != 0) return;   /* mip levels ignored */
    if (w < 0 || h < 0 || w > 4096 || h > 4096) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    uint32_t **pp = tex_level_ptr(t, target);
    free(*pp);
    *pp = calloc((size_t)(w ? w : 1) * (h ? h : 1), 4);
    if (target == GL_TEXTURE_2D) { t->w = w; t->h = h; } else { t->fw = w; t->fh = h; }
    if (!data || !*pp) return;
    int bpp = fmt_bpp(fmt, type);
    int stride = w * bpp;
    int al = g->unpack_align;
    if (al > 1) stride = (stride + al - 1) / al * al;
    const uint8_t *src = data;
    for (int y = 0; y < h; y++) {
        int dy = g->unpack_flip_y ? h - 1 - y : y;
        const uint8_t *s = src + (size_t)y * stride;
        uint32_t *d = *pp + (size_t)dy * w;
        if (fmt == GL_RGBA && type == GL_UNSIGNED_BYTE && !g->unpack_premul) memcpy(d, s, w * 4);
        else for (int x = 0; x < w; x++) { uint32_t p = conv_px(s + x * bpp, fmt, type); d[x] = g->unpack_premul ? premul(p) : p; }
    }
}

void sgl_tex_sub_image(SGL *g, int target, int level, int x0, int y0, int w, int h, int fmt, int type, const void *data)
{
    SGLTexture *t = bound_tex(g, target);
    if (!t || level != 0 || !data) return;
    uint32_t *px = *tex_level_ptr(t, target);
    int tw = target == GL_TEXTURE_2D ? t->w : t->fw, th = target == GL_TEXTURE_2D ? t->h : t->fh;
    if (!px || x0 < 0 || y0 < 0 || x0 + w > tw || y0 + h > th) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    int bpp = fmt_bpp(fmt, type);
    int stride = w * bpp, al = g->unpack_align;
    if (al > 1) stride = (stride + al - 1) / al * al;
    const uint8_t *src = data;
    for (int y = 0; y < h; y++) {
        int sy = g->unpack_flip_y ? h - 1 - y : y;
        const uint8_t *s = src + (size_t)sy * stride;
        uint32_t *d = px + (size_t)(y0 + y) * tw + x0;
        for (int x = 0; x < w; x++) { uint32_t p = conv_px(s + x * bpp, fmt, type); d[x] = g->unpack_premul ? premul(p) : p; }
    }
}

void sgl_tex_image_rgba(SGL *g, int target, int level, int w, int h, const uint8_t *rgba, int fmt)
{
    /* HTML image: top row first; WebGL uploads first row as t=0 unless FLIP_Y */
    SGLTexture *t = bound_tex(g, target);
    if (!t || level != 0) return;
    uint32_t **pp = tex_level_ptr(t, target);
    free(*pp);
    *pp = malloc((size_t)w * h * 4);
    if (!*pp) return;
    if (target == GL_TEXTURE_2D) { t->w = w; t->h = h; } else { t->fw = w; t->fh = h; }
    for (int y = 0; y < h; y++) {
        int dy = g->unpack_flip_y ? h - 1 - y : y;
        const uint32_t *s = (const uint32_t *)(rgba + (size_t)y * w * 4);
        uint32_t *d = *pp + (size_t)dy * w;
        for (int x = 0; x < w; x++) {
            uint32_t p = s[x];
            if (fmt == GL_RGB) p |= 0xFF000000u;
            else if (fmt == GL_LUMINANCE) { int l = p & 255; p = pack(l, l, l, 255); }
            else if (fmt == GL_ALPHA) p &= 0xFF000000u;
            if (g->unpack_premul) p = premul(p);
            d[x] = p;
        }
    }
}

void sgl_generate_mipmap(SGL *g, int target) { (void)bound_tex(g, target); }

/* ---------------- render target ---------------- */
typedef struct RT { int w, h; uint32_t *color; float *depth; float sx, sy; int isdef; } RT;

static void get_rt(SGL *g, RT *rt)
{
    memset(rt, 0, sizeof *rt);
    rt->sx = rt->sy = 1;
    if (!g->framebuffer) {
        rt->w = g->w; rt->h = g->h; rt->color = g->color; rt->depth = g->depth;
        rt->sx = g->sx; rt->sy = g->sy; rt->isdef = 1;
        return;
    }
    SGLFramebuffer *f = sgl_obj(&g->framebuffers, g->framebuffer);
    if (!f) return;
    SGLTexture *t = sgl_obj(&g->textures, f->color_tex);
    if (t) {
        if (f->color_tex_face) { rt->color = t->face[f->color_tex_face - GL_TEXTURE_CUBE_MAP_POSITIVE_X]; rt->w = t->fw; rt->h = t->fh; }
        else { rt->color = t->px; rt->w = t->w; rt->h = t->h; }
    } else {
        SGLRenderbuffer *r = sgl_obj(&g->renderbuffers, f->color_rb);
        if (r) { rt->color = r->color; rt->w = r->w; rt->h = r->h; }
    }
    SGLRenderbuffer *d = sgl_obj(&g->renderbuffers, f->depth_rb);
    if (d && d->depth && (!rt->color || (d->w == rt->w && d->h == rt->h))) {
        rt->depth = d->depth;
        if (!rt->color) { rt->w = d->w; rt->h = d->h; }
    }
}

void sgl_copy_tex_image(SGL *g, int target, int level, int x, int y, int w, int h)
{
    RT rt; get_rt(g, &rt);
    if (!rt.color || level) return;
    uint8_t *buf = malloc((size_t)w * h * 4);
    if (!buf) return;
    sgl_read_pixels(g, x, y, w, h, buf);
    int flip = g->unpack_flip_y; g->unpack_flip_y = 0;
    int al = g->unpack_align; g->unpack_align = 1;
    sgl_tex_image(g, target, 0, GL_RGBA, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    g->unpack_flip_y = flip; g->unpack_align = al;
    free(buf);
}

/* ---------------- shaders / programs ---------------- */
int sgl_create_shader(SGL *g, int type)
{
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER) { sgl_set_error(g, GL_INVALID_ENUM); return 0; }
    SGLShader *s = calloc(1, sizeof(SGLShader));
    s->type = type;
    s->src = strdup("");
    s->log = strdup("");
    return sgl_create_obj(g, &g->shaders, s);
}
void sgl_shader_source(SGL *g, int id, const char *src)
{
    SGLShader *s = sgl_obj(&g->shaders, id);
    if (!s) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    free(s->src); s->src = strdup(src ? src : "");
}
void sgl_compile_shader(SGL *g, int id)
{
    SGLShader *s = sgl_obj(&g->shaders, id);
    if (!s) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    GShader *gs = glsl_compile(s->src, s->type == GL_FRAGMENT_SHADER);
    s->compiled = 1; s->compile_ok = gs->ok;
    free(s->log); s->log = strdup(gs->log ? gs->log : "");
    glsl_free(gs);
}
void sgl_delete_shader(SGL *g, int id)
{
    SGLShader *s = sgl_obj(&g->shaders, id);
    if (!s) return;
    /* keep source alive if attached: programs keep their own copies at link */
    s->deleted = 1;
}
int sgl_create_program(SGL *g) { return sgl_create_obj(g, &g->programs, calloc(1, sizeof(SGLProgram))); }
void sgl_attach_shader(SGL *g, int prog, int sh)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    SGLShader *s = sgl_obj(&g->shaders, sh);
    if (!p || !s) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    if (s->type == GL_VERTEX_SHADER) p->vs = sh; else p->fs = sh;
}
void sgl_detach_shader(SGL *g, int prog, int sh)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (!p) return;
    if (p->vs == sh) p->vs = 0;
    if (p->fs == sh) p->fs = 0;
}
void sgl_bind_attrib_location(SGL *g, int prog, int loc, const char *name)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (!p || loc < 0 || loc >= SGL_MAX_ATTRIBS) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    for (int i = 0; i < SGL_MAX_ATTRIBS; i++) if (p->bind_names[i] && !strcmp(p->bind_names[i], name)) { free(p->bind_names[i]); p->bind_names[i] = NULL; }
    free(p->bind_names[loc]); p->bind_names[loc] = strdup(name);
}

static int gl_type_of(GType t)
{
    if (t.kind == GK_SAMPLER2D) return GL_SAMPLER_2D;
    if (t.kind == GK_SAMPLERCUBE) return GL_SAMPLER_CUBE;
    if (t.mat) return t.mat == 2 ? GL_FLOAT_MAT2 : t.mat == 3 ? GL_FLOAT_MAT3 : GL_FLOAT_MAT4;
    static const int f[] = { GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT_VEC4 };
    static const int i[] = { GL_INT, GL_INT_VEC2, GL_INT_VEC3, GL_INT_VEC4 };
    static const int b[] = { GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4 };
    int v = t.vec ? t.vec - 1 : 0;
    return t.kind == GK_FLOAT ? f[v] : t.kind == GK_INT ? i[v] : b[v];
}

static void plog(SGLProgram *p, const char *msg)
{
    size_t a = p->log ? strlen(p->log) : 0, b = strlen(msg);
    p->log = realloc(p->log, a + b + 2);
    memcpy(p->log + a, msg, b); p->log[a + b] = '\n'; p->log[a + b + 1] = 0;
}

void sgl_link_program(SGL *g, int prog)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (!p) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    free_prog_link(p);
    free(p->log); p->log = strdup("");
    p->linked = 1; p->link_ok = 0;
    SGLShader *vs = sgl_obj(&g->shaders, p->vs), *fs = sgl_obj(&g->shaders, p->fs);
    if (!vs || !fs) { plog(p, "missing shader"); return; }
#ifndef _TINSPIRE
    if (getenv("NSPGL_DUMPSH")) {
        static int nd; char fn[256];
        snprintf(fn, sizeof fn, "%s/sh%d.vs", getenv("NSPGL_DUMPSH"), nd); FILE *f = fopen(fn, "w"); if (f) { fputs(vs->src, f); fclose(f); }
        snprintf(fn, sizeof fn, "%s/sh%d.fs", getenv("NSPGL_DUMPSH"), nd++); f = fopen(fn, "w"); if (f) { fputs(fs->src, f); fclose(f); }
    }
#endif
    p->v = glsl_compile(vs->src, 0);
    p->f = glsl_compile(fs->src, 1);
    if (!p->v->ok || !p->f->ok) { plog(p, "shader compile failed"); plog(p, p->v->log); plog(p, p->f->log); free_prog_link(p); return; }
    GShader *V = p->v, *F = p->f;
    /* varyings */
    int nv = 0, vvs[MAXV], vfs[MAXV];
    for (int i = 0; i < F->nvars; i++) {
        GVar *fv = &F->vars[i];
        if (fv->storage != GS_VARYING) continue;
        GVar *match = NULL;
        for (int k = 0; k < V->nvars; k++) if (V->vars[k].storage == GS_VARYING && !strcmp(V->vars[k].name, fv->name)) match = &V->vars[k];
        int sz = glsl_type_size(fv->type);
        if (match && glsl_type_size(match->type) != sz) { plog(p, "varying type mismatch"); free_prog_link(p); return; }
        for (int c = 0; c < sz; c++) {
            if (nv >= MAXV) { plog(p, "too many varyings"); free_prog_link(p); return; }
            vvs[nv] = match ? match->off + c : -1;
            vfs[nv] = fv->off + c;
            nv++;
        }
    }
    p->nvary = nv;
    p->vary_vs = malloc(sizeof(int) * (nv + 1)); p->vary_fs = malloc(sizeof(int) * (nv + 1));
    memcpy(p->vary_vs, vvs, sizeof(int) * nv); memcpy(p->vary_fs, vfs, sizeof(int) * nv);
    /* attributes */
    p->attrs = calloc(V->nvars + 1, sizeof(SGLAttrib));
    int used = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < V->nvars; i++) {
            GVar *a = &V->vars[i];
            if (a->storage != GS_ATTRIBUTE) continue;
            int cols = a->type.mat ? a->type.mat : 1;
            int bound = -1;
            for (int l = 0; l < SGL_MAX_ATTRIBS; l++) if (p->bind_names[l] && !strcmp(p->bind_names[l], a->name)) bound = l;
            if (pass == 0 && bound < 0) continue;
            if (pass == 1 && bound >= 0) continue;
            int loc = bound;
            if (loc < 0) {
                for (loc = 0; loc + cols <= SGL_MAX_ATTRIBS; loc++) {
                    int ok = 1; for (int c = 0; c < cols; c++) if (used & (1 << (loc + c))) ok = 0;
                    if (ok) break;
                }
                if (loc + cols > SGL_MAX_ATTRIBS) { plog(p, "too many attributes"); free_prog_link(p); return; }
            }
            for (int c = 0; c < cols; c++) used |= 1 << (loc + c);
            SGLAttrib *at = &p->attrs[p->nattrs++];
            snprintf(at->name, sizeof at->name, "%s", a->name);
            at->type = a->type; at->vs_off = a->off; at->loc = loc; at->gltype = gl_type_of(a->type);
        }
    /* uniforms */
    p->unis = calloc(V->nvars + F->nvars + 1, sizeof(SGLUniform));
    for (int s = 0; s < 2; s++) {
        GShader *S = s ? F : V;
        for (int i = 0; i < S->nvars; i++) {
            GVar *u = &S->vars[i];
            if (u->storage != GS_UNIFORM) continue;
            SGLUniform *U = NULL;
            for (int k = 0; k < p->nunis; k++) if (!strcmp(p->unis[k].name, u->name)) U = &p->unis[k];
            if (!U) {
                U = &p->unis[p->nunis++];
                snprintf(U->name, sizeof U->name, "%s", u->name);
                U->type = u->type; U->vs_off = U->fs_off = -1;
                GType e = u->type; e.arr = 0;
                U->esize = glsl_type_size(e); U->count = u->type.arr ? u->type.arr : 1;
                U->gltype = gl_type_of(u->type);
            } else if (glsl_type_size(U->type) != glsl_type_size(u->type)) {
                plog(p, "uniform type mismatch between shaders"); free_prog_link(p); return;
            }
            if (s) U->fs_off = u->off; else U->vs_off = u->off;
        }
    }
    p->vmem = malloc(sizeof(float) * V->memsize); memcpy(p->vmem, V->init, sizeof(float) * V->memsize);
    p->fmem = malloc(sizeof(float) * F->memsize); memcpy(p->fmem, F->init, sizeof(float) * F->memsize);
    p->link_ok = 1;
}
void sgl_use_program(SGL *g, int prog)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (prog && (!p || !p->link_ok)) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    g->program = prog;
}
void sgl_delete_program(SGL *g, int prog)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (!p) return;
    if (g->program == prog) { p->deleted = 1; return; }
    free_prog_link(p); free(p->log);
    for (int k = 0; k < SGL_MAX_ATTRIBS; k++) free(p->bind_names[k]);
    free(p); obj_clear(&g->programs, prog);
}
int sgl_get_attrib_location(SGL *g, int prog, const char *name)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (!p || !p->link_ok) return -1;
    for (int i = 0; i < p->nattrs; i++) if (!strcmp(p->attrs[i].name, name)) return p->attrs[i].loc;
    return -1;
}
int sgl_get_uniform_location(SGL *g, int prog, const char *name, int *elem)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    *elem = 0;
    if (!p || !p->link_ok) return -1;
    char base[64]; snprintf(base, sizeof base, "%s", name);
    int idx = 0;
    char *br = strrchr(base, '[');
    if (br && base[strlen(base) - 1] == ']' && !strchr(br, '.')) { idx = atoi(br + 1); *br = 0; }
    for (int i = 0; i < p->nunis; i++) if (!strcmp(p->unis[i].name, base)) {
        if (idx >= p->unis[i].count) return -1;
        *elem = idx; return i;
    }
    if (br) {   /* maybe exact name like s[0].f */
        for (int i = 0; i < p->nunis; i++) if (!strcmp(p->unis[i].name, name)) return i;
    }
    return -1;
}
void sgl_uniform(SGL *g, int prog, int uidx, int elem, const float *v, int nfloats)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    if (!p || !p->link_ok || uidx < 0 || uidx >= p->nunis) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    SGLUniform *U = &p->unis[uidx];
    int maxf = (U->count - elem) * U->esize;
    if (nfloats > maxf) nfloats = maxf;
    if (nfloats <= 0) return;
    int start = elem * U->esize;
    if (U->vs_off >= 0) memcpy(p->vmem + U->vs_off + start, v, nfloats * sizeof(float));
    if (U->fs_off >= 0) memcpy(p->fmem + U->fs_off + start, v, nfloats * sizeof(float));
}
void sgl_get_uniform(SGL *g, int prog, int uidx, int elem, float *out, int *n)
{
    SGLProgram *p = sgl_obj(&g->programs, prog);
    *n = 0;
    if (!p || !p->link_ok || uidx < 0 || uidx >= p->nunis) return;
    SGLUniform *U = &p->unis[uidx];
    const float *src = U->vs_off >= 0 ? p->vmem + U->vs_off : p->fmem + U->fs_off;
    memcpy(out, src + elem * U->esize, U->esize * sizeof(float));
    *n = U->esize;
}

/* ---------------- vertex attributes / state ---------------- */
void sgl_vertex_attrib_pointer(SGL *g, int idx, int size, int type, int norm, int stride, int offset)
{
    if (idx < 0 || idx >= SGL_MAX_ATTRIBS || size < 1 || size > 4) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    if (!g->array_buffer) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    SGLAttribState *a = &g->attr[idx];
    a->buffer = g->array_buffer; a->size = size; a->type = type; a->normalized = norm; a->stride = stride; a->offset = offset;
}
void sgl_enable_attrib(SGL *g, int idx, int en)
{
    if (idx < 0 || idx >= SGL_MAX_ATTRIBS) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    g->attr[idx].enabled = en;
}
void sgl_vertex_attrib(SGL *g, int idx, const float *v, int n)
{
    if (idx < 0 || idx >= SGL_MAX_ATTRIBS) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    float *d = g->attr[idx].generic;
    d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 1;
    for (int i = 0; i < n && i < 4; i++) d[i] = v[i];
}

static int *cap_ptr(SGL *g, int cap)
{
    switch (cap) {
    case GL_CULL_FACE: return &g->cull;
    case GL_BLEND: return &g->blend;
    case GL_DEPTH_TEST: return &g->depth_test;
    case GL_SCISSOR_TEST: return &g->scissor_test;
    case GL_STENCIL_TEST: return &g->stencil_test;
    case GL_DITHER: return &g->dither;
    case GL_POLYGON_OFFSET_FILL: return &g->poly_offset;
    case 0x809E: case 0x80A0: { static int dummy; return &dummy; }
    }
    return NULL;
}
void sgl_enable(SGL *g, int cap, int en)
{
    int *p = cap_ptr(g, cap);
    if (!p) { sgl_set_error(g, GL_INVALID_ENUM); return; }
    *p = en;
}
int sgl_is_enabled(SGL *g, int cap) { int *p = cap_ptr(g, cap); return p ? *p : 0; }
void sgl_viewport(SGL *g, int x, int y, int w, int h)
{
    if (w < 0 || h < 0) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    g->viewport[0] = x; g->viewport[1] = y; g->viewport[2] = w; g->viewport[3] = h;
}
void sgl_scissor(SGL *g, int x, int y, int w, int h)
{
    if (w < 0 || h < 0) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    g->scissor[0] = x; g->scissor[1] = y; g->scissor[2] = w; g->scissor[3] = h;
}

/* pixel bounds (real pixels) of scissor intersected with target */
static void bounds(SGL *g, RT *rt, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = 0; *y0 = 0; *x1 = rt->w; *y1 = rt->h;
    if (g->scissor_test) {
        int sx0 = (int)floorf(g->scissor[0] * rt->sx + 0.5f), sy0 = (int)floorf(g->scissor[1] * rt->sy + 0.5f);
        int sx1 = (int)floorf((g->scissor[0] + g->scissor[2]) * rt->sx + 0.5f), sy1 = (int)floorf((g->scissor[1] + g->scissor[3]) * rt->sy + 0.5f);
        if (sx0 > *x0) *x0 = sx0; if (sy0 > *y0) *y0 = sy0;
        if (sx1 < *x1) *x1 = sx1; if (sy1 < *y1) *y1 = sy1;
    }
}

void sgl_clear(SGL *g, int mask)
{
    RT rt; get_rt(g, &rt);
    int x0, y0, x1, y1;
    bounds(g, &rt, &x0, &y0, &x1, &y1);
    if (x1 <= x0 || y1 <= y0) return;
    if ((mask & GL_COLOR_BUFFER_BIT) && rt.color) {
        uint32_t c = pack(cl8f(g->clear_color[0]), cl8f(g->clear_color[1]), cl8f(g->clear_color[2]),
                          cl8f(g->has_alpha || !rt.isdef ? g->clear_color[3] : 1.0f));
        uint32_t keep = 0;
        for (int i = 0; i < 4; i++) if (!g->color_mask[i]) keep |= 0xFFu << (8 * i);
        for (int y = y0; y < y1; y++) {
            uint32_t *row = rt.color + (size_t)y * rt.w;
            if (!keep) for (int x = x0; x < x1; x++) row[x] = c;
            else for (int x = x0; x < x1; x++) row[x] = (row[x] & keep) | (c & ~keep);
        }
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) && rt.depth && g->depth_mask) {
        float d = g->clear_depth < 0 ? 0 : g->clear_depth > 1 ? 1 : g->clear_depth;
        for (int y = y0; y < y1; y++) { float *row = rt.depth + (size_t)y * rt.w; for (int x = x0; x < x1; x++) row[x] = d; }
    }
}

void sgl_read_pixels(SGL *g, int x, int y, int w, int h, uint8_t *out)
{
    RT rt; get_rt(g, &rt);
    int stride = w * 4;
    if (g->pack_align > 1) stride = (stride + g->pack_align - 1) / g->pack_align * g->pack_align;
    for (int j = 0; j < h; j++) {
        uint8_t *o = out + (size_t)j * stride;
        for (int i = 0; i < w; i++) {
            int px = (int)((x + i + 0.5f) * rt.sx), py = (int)((y + j + 0.5f) * rt.sy);
            uint32_t c = 0;
            if (rt.color && px >= 0 && py >= 0 && px < rt.w && py < rt.h) c = rt.color[(size_t)py * rt.w + px];
            if (rt.isdef && !g->has_alpha) c |= 0xFF000000u;
            memcpy(o + i * 4, &c, 4);
        }
    }
}

/* ---------------- framebuffers ---------------- */
int sgl_create_framebuffer(SGL *g) { return sgl_create_obj(g, &g->framebuffers, calloc(1, sizeof(SGLFramebuffer))); }
void sgl_delete_framebuffer(SGL *g, int id)
{
    void *f = sgl_obj(&g->framebuffers, id);
    if (!f) return;
    free(f); obj_clear(&g->framebuffers, id);
    if (g->framebuffer == id) g->framebuffer = 0;
}
void sgl_bind_framebuffer(SGL *g, int id) { g->framebuffer = id; }
void sgl_framebuffer_texture(SGL *g, int attach, int textarget, int tex)
{
    SGLFramebuffer *f = sgl_obj(&g->framebuffers, g->framebuffer);
    if (!f) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    if (attach == GL_COLOR_ATTACHMENT0) {
        f->color_tex = tex; f->color_rb = 0;
        f->color_tex_face = textarget == GL_TEXTURE_2D ? 0 : textarget;
    }
}
void sgl_framebuffer_renderbuffer(SGL *g, int attach, int rb)
{
    SGLFramebuffer *f = sgl_obj(&g->framebuffers, g->framebuffer);
    if (!f) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    if (attach == GL_COLOR_ATTACHMENT0) { f->color_rb = rb; f->color_tex = 0; }
    else if (attach == GL_DEPTH_ATTACHMENT || attach == GL_DEPTH_STENCIL_ATTACHMENT) f->depth_rb = rb;
    else if (attach == GL_STENCIL_ATTACHMENT) f->stencil_rb = rb;
}
int sgl_check_framebuffer(SGL *g)
{
    if (!g->framebuffer) return GL_FRAMEBUFFER_COMPLETE;
    RT rt; get_rt(g, &rt);
    if (!rt.color && !rt.depth) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    if (rt.w <= 0 || rt.h <= 0) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    return GL_FRAMEBUFFER_COMPLETE;
}
int sgl_create_renderbuffer(SGL *g) { return sgl_create_obj(g, &g->renderbuffers, calloc(1, sizeof(SGLRenderbuffer))); }
void sgl_delete_renderbuffer(SGL *g, int id)
{
    SGLRenderbuffer *r = sgl_obj(&g->renderbuffers, id);
    if (!r) return;
    free(r->depth); free(r->color); free(r); obj_clear(&g->renderbuffers, id);
    if (g->renderbuffer == id) g->renderbuffer = 0;
}
void sgl_bind_renderbuffer(SGL *g, int id) { g->renderbuffer = id; }
void sgl_renderbuffer_storage(SGL *g, int fmt, int w, int h)
{
    SGLRenderbuffer *r = sgl_obj(&g->renderbuffers, g->renderbuffer);
    if (!r) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    free(r->depth); free(r->color); r->depth = NULL; r->color = NULL;
    r->w = w; r->h = h; r->format = fmt;
    if (fmt == GL_DEPTH_COMPONENT16 || fmt == GL_DEPTH_STENCIL || fmt == GL_DEPTH_COMPONENT) {
        r->depth = malloc((size_t)w * h * sizeof(float) + 4);
        for (int i = 0; i < w * h; i++) r->depth[i] = 1.0f;
    } else if (fmt != GL_STENCIL_INDEX8) r->color = calloc((size_t)w * h + 1, 4);
}

/* =====================================================================
 *                           RASTERIZER
 * ===================================================================== */
typedef struct AttrFetch {
    const uint8_t *data; int bufsize;
    int offset, stride, size, type, norm;
    int vs_off;        /* destination in vs memory */
    int ncomp;         /* components of the shader attribute column */
    const float *generic;
    int enabled;
} AttrFetch;

typedef struct DC {
    SGL *g; SGLProgram *p; RT rt;
    GShader *vs, *fs;
    float *vmem, *fmem;
    int nv, vstride;
    AttrFetch at[SGL_MAX_ATTRIBS * 4]; int nat;
    float vpx, vpy, vpw, vph, zn, zf;
    int cx0, cy0, cx1, cy1;
    int o_color, o_fragcoord, o_front, o_pcoord;
    int *vfs;
    float fcx, fcy;    /* fragcoord scale (virtual / real) */
    int depth_test, depth_write, depth_func;
    int blend; uint32_t keepmask;
    int discard;
} DC;

static void sampler_cb(void *user, int unit, int cube, const float *c, float bias, float *out);

static int prep_draw(SGL *g, DC *dc)
{
    memset(dc, 0, sizeof *dc);
    dc->g = g;
    SGLProgram *p = sgl_obj(&g->programs, g->program);
    if (!p || !p->link_ok) { sgl_set_error(g, GL_INVALID_OPERATION); return 0; }
    dc->p = p; dc->vs = p->v; dc->fs = p->f; dc->vmem = p->vmem; dc->fmem = p->fmem;
    get_rt(g, &dc->rt);
    if (!dc->rt.color && !dc->rt.depth) return 0;
    if (dc->rt.w <= 0 || dc->rt.h <= 0) return 0;
    dc->nv = p->nvary; dc->vstride = 4 + p->nvary; dc->vfs = p->vary_fs;
    for (int i = 0; i < p->nattrs; i++) {
        SGLAttrib *a = &p->attrs[i];
        int cols = a->type.mat ? a->type.mat : 1;
        int ncomp = a->type.mat ? a->type.vec : a->type.vec;
        for (int c = 0; c < cols; c++) {
            AttrFetch *f = &dc->at[dc->nat++];
            SGLAttribState *s = &g->attr[a->loc + c];
            f->vs_off = a->vs_off + c * ncomp;
            f->ncomp = ncomp;
            f->generic = s->generic;
            f->enabled = s->enabled;
            if (s->enabled) {
                SGLBuffer *b = sgl_obj(&g->buffers, s->buffer);
                if (!b) { sgl_set_error(g, GL_INVALID_OPERATION); return 0; }
                f->data = b->data; f->bufsize = b->size;
                f->offset = s->offset; f->size = s->size; f->type = s->type; f->norm = s->normalized;
                int tsz = (s->type == GL_FLOAT || s->type == GL_INT || s->type == GL_UNSIGNED_INT) ? 4 :
                          (s->type == GL_SHORT || s->type == GL_UNSIGNED_SHORT || s->type == GL_HALF_FLOAT_OES) ? 2 : 1;
                f->stride = s->stride ? s->stride : tsz * s->size;
                f->ncomp = ncomp;
                /* store element size in norm high bits */
                f->norm = s->normalized | (tsz << 4);
            }
        }
    }
    RT *rt = &dc->rt;
    dc->vpx = g->viewport[0] * rt->sx; dc->vpy = g->viewport[1] * rt->sy;
    dc->vpw = g->viewport[2] * rt->sx; dc->vph = g->viewport[3] * rt->sy;
    dc->zn = g->depth_range[0]; dc->zf = g->depth_range[1];
    bounds(g, rt, &dc->cx0, &dc->cy0, &dc->cx1, &dc->cy1);
    /* clip to viewport */
    int vx0 = (int)floorf(dc->vpx + 0.5f), vy0 = (int)floorf(dc->vpy + 0.5f);
    int vx1 = (int)floorf(dc->vpx + dc->vpw + 0.5f), vy1 = (int)floorf(dc->vpy + dc->vph + 0.5f);
    if (vx0 > dc->cx0) dc->cx0 = vx0; if (vy0 > dc->cy0) dc->cy0 = vy0;
    if (vx1 < dc->cx1) dc->cx1 = vx1; if (vy1 < dc->cy1) dc->cy1 = vy1;
    dc->o_color = dc->fs->off_fragcolor; dc->o_fragcoord = dc->fs->off_fragcoord;
    dc->o_front = dc->fs->off_frontfacing; dc->o_pcoord = dc->fs->off_pointcoord;
    dc->fcx = 1.0f / rt->sx; dc->fcy = 1.0f / rt->sy;
    dc->depth_test = g->depth_test && rt->depth;
    dc->depth_write = dc->depth_test && g->depth_mask;
    dc->depth_func = g->depth_func;
    dc->blend = g->blend;
    dc->keepmask = 0;
    for (int i = 0; i < 4; i++) if (!g->color_mask[i]) dc->keepmask |= 0xFFu << (8 * i);
    if (rt->isdef && !g->has_alpha) dc->keepmask &= 0x00FFFFFFu;
    dc->discard = dc->fs->has_discard;
    return dc->cx1 > dc->cx0 && dc->cy1 > dc->cy0;
}

static inline float fetch_comp(const AttrFetch *f, const uint8_t *p, int j)
{
    int tsz = f->norm >> 4, norm = f->norm & 1;
    const uint8_t *q = p + j * tsz;
    switch (f->type) {
    case GL_FLOAT: { float v; memcpy(&v, q, 4); return v; }
    case GL_UNSIGNED_BYTE: return norm ? q[0] * (1.0f / 255.0f) : q[0];
    case GL_BYTE: { int8_t v = (int8_t)q[0]; float r = norm ? v * (1.0f / 127.0f) : v; return r < -1 && norm ? -1 : r; }
    case GL_UNSIGNED_SHORT: { uint16_t v; memcpy(&v, q, 2); return norm ? v * (1.0f / 65535.0f) : v; }
    case GL_SHORT: { int16_t v; memcpy(&v, q, 2); float r = norm ? v * (1.0f / 32767.0f) : v; return r < -1 && norm ? -1 : r; }
    case GL_HALF_FLOAT_OES: { uint16_t v; memcpy(&v, q, 2); return half2f(v); }
    case GL_INT: { int32_t v; memcpy(&v, q, 4); return (float)v; }
    case GL_UNSIGNED_INT: { uint32_t v; memcpy(&v, q, 4); return (float)v; }
    }
    return 0;
}

static void run_vs(DC *dc, int idx, float *out)
{
    float *m = dc->vmem;
    for (int i = 0; i < dc->nat; i++) {
        AttrFetch *f = &dc->at[i];
        float *d = m + f->vs_off;
        if (!f->enabled) { for (int j = 0; j < f->ncomp; j++) d[j] = f->generic[j]; continue; }
        long pos = (long)f->offset + (long)idx * f->stride;
        int need = (f->norm >> 4) * f->size;
        if (idx < 0 || pos < 0 || pos + need > f->bufsize) { for (int j = 0; j < f->ncomp; j++) d[j] = j == 3; continue; }
        const uint8_t *p = f->data + pos;
        for (int j = 0; j < f->ncomp; j++) d[j] = j < f->size ? fetch_comp(f, p, j) : (j == 3 ? 1.0f : 0.0f);
    }
    glsl_run(dc->vs, m, sampler_cb, dc);
    const float *pos = m + dc->vs->off_position;
    out[0] = pos[0]; out[1] = pos[1]; out[2] = pos[2]; out[3] = pos[3];
    const int *vv = dc->p->vary_vs;
    for (int k = 0; k < dc->nv; k++) out[4 + k] = vv[k] >= 0 ? m[vv[k]] : 0.0f;
}

/* ---------------- texture sampling ---------------- */
static inline int wrap_i(int i, int n, int mode)
{
    if (mode == GL_CLAMP_TO_EDGE) return i < 0 ? 0 : i >= n ? n - 1 : i;
    if (mode == GL_MIRRORED_REPEAT) {
        int p = n * 2;
        i %= p; if (i < 0) i += p;
        return i < n ? i : p - 1 - i;
    }
    i %= n; if (i < 0) i += n;
    return i;
}

static void sample_img(const uint32_t *px, int w, int h, int ws, int wt, int linear, float s, float t, float *out)
{
    if (!px || w <= 0 || h <= 0) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
    if (!linear) {
        int x = (int)floorf(s * w), y = (int)floorf(t * h);
        if (s != s) x = 0;
        if (t != t) y = 0;
        x = wrap_i(x, w, ws); y = wrap_i(y, h, wt);
        uint32_t c = px[(size_t)y * w + x];
        const float k = 1.0f / 255.0f;
        out[0] = (c & 255) * k; out[1] = ((c >> 8) & 255) * k; out[2] = ((c >> 16) & 255) * k; out[3] = (c >> 24) * k;
        return;
    }
    float fx = s * w - 0.5f, fy = t * h - 0.5f;
    if (fx != fx) fx = 0;
    if (fy != fy) fy = 0;
    if (fx > 1e7f) fx = 1e7f; if (fx < -1e7f) fx = -1e7f;
    if (fy > 1e7f) fy = 1e7f; if (fy < -1e7f) fy = -1e7f;
    int ix = (int)floorf(fx), iy = (int)floorf(fy);
    int ax = (int)((fx - ix) * 256.0f), ay = (int)((fy - iy) * 256.0f);
    int x0 = wrap_i(ix, w, ws), x1 = wrap_i(ix + 1, w, ws), y0 = wrap_i(iy, h, wt), y1 = wrap_i(iy + 1, h, wt);
    uint32_t c00 = px[(size_t)y0 * w + x0], c10 = px[(size_t)y0 * w + x1], c01 = px[(size_t)y1 * w + x0], c11 = px[(size_t)y1 * w + x1];
    int w00 = (256 - ax) * (256 - ay), w10 = ax * (256 - ay), w01 = (256 - ax) * ay, w11 = ax * ay;
    const float k = 1.0f / (255.0f * 65536.0f);
    for (int ch = 0; ch < 4; ch++) {
        int sh = ch * 8;
        int v = ((c00 >> sh) & 255) * w00 + ((c10 >> sh) & 255) * w10 + ((c01 >> sh) & 255) * w01 + ((c11 >> sh) & 255) * w11;
        out[ch] = v * k;
    }
}

static void sampler_cb(void *user, int unit, int cube, const float *c, float bias, float *out)
{
    DC *dc = user;
    SGL *g = dc->g;
    (void)bias;
    if (unit < 0 || unit >= SGL_MAX_UNITS) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
    SGLTexture *t = sgl_obj(&g->textures, cube ? g->texcube[unit] : g->tex2d[unit]);
    if (!t) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
    int linear = (t->magf == GL_LINEAR);   /* use mag filter (no mip chain) */
    if (t->minf == GL_NEAREST || t->minf == GL_NEAREST_MIPMAP_NEAREST || t->minf == GL_NEAREST_MIPMAP_LINEAR) {
        if (t->magf == GL_NEAREST) linear = 0;
    }
    if (!cube) { sample_img(t->px, t->w, t->h, t->wraps, t->wrapt, linear, c[0], c[1], out); return; }
    float rx = c[0], ry = c[1], rz = c[2];
    float ax = fabsf(rx), ay = fabsf(ry), az = fabsf(rz), sc, tc, ma; int face;
    if (ax >= ay && ax >= az) { ma = ax; if (rx > 0) { face = 0; sc = -rz; tc = -ry; } else { face = 1; sc = rz; tc = -ry; } }
    else if (ay >= az) { ma = ay; if (ry > 0) { face = 2; sc = rx; tc = rz; } else { face = 3; sc = rx; tc = -rz; } }
    else { ma = az; if (rz > 0) { face = 4; sc = rx; tc = -ry; } else { face = 5; sc = -rx; tc = -ry; } }
    if (ma == 0) ma = 1;
    float s = (sc / ma + 1) * 0.5f, tt = (tc / ma + 1) * 0.5f;
    sample_img(t->face[face], t->fw, t->fh, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, linear, s, tt, out);
}

/* ---------------- fragment output ---------------- */
static inline void bfactor(int mode, const float *s, const float *d, const float *k, float *o)
{
    switch (mode) {
    case GL_ZERO: o[0] = o[1] = o[2] = o[3] = 0; break;
    case GL_ONE: o[0] = o[1] = o[2] = o[3] = 1; break;
    case GL_SRC_COLOR: o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = s[3]; break;
    case GL_ONE_MINUS_SRC_COLOR: o[0] = 1 - s[0]; o[1] = 1 - s[1]; o[2] = 1 - s[2]; o[3] = 1 - s[3]; break;
    case GL_DST_COLOR: o[0] = d[0]; o[1] = d[1]; o[2] = d[2]; o[3] = d[3]; break;
    case GL_ONE_MINUS_DST_COLOR: o[0] = 1 - d[0]; o[1] = 1 - d[1]; o[2] = 1 - d[2]; o[3] = 1 - d[3]; break;
    case GL_SRC_ALPHA: o[0] = o[1] = o[2] = o[3] = s[3]; break;
    case GL_ONE_MINUS_SRC_ALPHA: o[0] = o[1] = o[2] = o[3] = 1 - s[3]; break;
    case GL_DST_ALPHA: o[0] = o[1] = o[2] = o[3] = d[3]; break;
    case GL_ONE_MINUS_DST_ALPHA: o[0] = o[1] = o[2] = o[3] = 1 - d[3]; break;
    case GL_CONSTANT_COLOR: o[0] = k[0]; o[1] = k[1]; o[2] = k[2]; o[3] = k[3]; break;
    case GL_ONE_MINUS_CONSTANT_COLOR: o[0] = 1 - k[0]; o[1] = 1 - k[1]; o[2] = 1 - k[2]; o[3] = 1 - k[3]; break;
    case GL_CONSTANT_ALPHA: o[0] = o[1] = o[2] = o[3] = k[3]; break;
    case GL_ONE_MINUS_CONSTANT_ALPHA: o[0] = o[1] = o[2] = o[3] = 1 - k[3]; break;
    case GL_SRC_ALPHA_SATURATE: { float f = s[3] < 1 - d[3] ? s[3] : 1 - d[3]; o[0] = o[1] = o[2] = f; o[3] = 1; break; }
    default: o[0] = o[1] = o[2] = o[3] = 1;
    }
}

static inline void write_frag(DC *dc, uint32_t *dst, const float *src)
{
    uint32_t out;
    if (!dc->blend) {
        int c[4];
        for (int i = 0; i < 4; i++) { int v = (int)(src[i] * 255.0f + 0.5f); c[i] = v < 0 ? 0 : v > 255 ? 255 : v; }
        out = pack(c[0], c[1], c[2], c[3]);
        if (dc->keepmask) out = (*dst & dc->keepmask) | (out & ~dc->keepmask);
        *dst = out;
        return;
    }
    float s[4];
    for (int i = 0; i < 4; i++) { float v = src[i]; s[i] = v < 0 ? 0 : v > 1 ? 1 : (v == v ? v : 0); }
    if (dc->blend) {
        SGL *g = dc->g;
        uint32_t dp = *dst;
        const float k = 1.0f / 255.0f;
        float d[4] = { (dp & 255) * k, ((dp >> 8) & 255) * k, ((dp >> 16) & 255) * k, (dp >> 24) * k };
        if (dc->rt.isdef && !g->has_alpha) d[3] = 1;
        float fs[4], fd[4], fsa[4], fda[4], r[4];
        bfactor(g->bsrc_rgb, s, d, g->blend_color, fs);
        bfactor(g->bdst_rgb, s, d, g->blend_color, fd);
        bfactor(g->bsrc_a, s, d, g->blend_color, fsa);
        bfactor(g->bdst_a, s, d, g->blend_color, fda);
        for (int i = 0; i < 3; i++) {
            float a = s[i] * fs[i], b = d[i] * fd[i];
            r[i] = g->beq_rgb == GL_FUNC_SUBTRACT ? a - b : g->beq_rgb == GL_FUNC_REVERSE_SUBTRACT ? b - a : a + b;
        }
        { float a = s[3] * fsa[3], b = d[3] * fda[3];
          r[3] = g->beq_a == GL_FUNC_SUBTRACT ? a - b : g->beq_a == GL_FUNC_REVERSE_SUBTRACT ? b - a : a + b; }
        out = pack(cl8f(r[0]), cl8f(r[1]), cl8f(r[2]), cl8f(r[3]));
    } else {
        out = pack((int)(s[0] * 255 + 0.5f), (int)(s[1] * 255 + 0.5f), (int)(s[2] * 255 + 0.5f), (int)(s[3] * 255 + 0.5f));
    }
    if (dc->keepmask) out = (*dst & dc->keepmask) | (out & ~dc->keepmask);
    *dst = out;
}

static inline int depth_pass(int func, float z, float d)
{
    switch (func) {
    case GL_NEVER: return 0; case GL_LESS: return z < d; case GL_EQUAL: return z == d; case GL_LEQUAL: return z <= d;
    case GL_GREATER: return z > d; case GL_NOTEQUAL: return z != d; case GL_GEQUAL: return z >= d;
    }
    return 1;
}

/* run fragment shader for pixel (x,y) with varyings already in fmem; returns 1 if written */
static inline void shade_pixel_t(DC *dc, int x, int y, float z, float invw, int tested)
{
    RT *rt = &dc->rt;
    size_t pi = (size_t)y * rt->w + x;
    if (!tested && dc->depth_test && !depth_pass(dc->depth_func, z, rt->depth[pi])) return;
    float *m = dc->fmem;
    if (dc->o_fragcoord >= 0) {
        float *fc = m + dc->o_fragcoord;
        fc[0] = (x + 0.5f) * dc->fcx; fc[1] = (y + 0.5f) * dc->fcy; fc[2] = z; fc[3] = invw;
    }
    int r = glsl_run(dc->fs, m, sampler_cb, dc);
    if (r) return;
    if (dc->depth_write) rt->depth[pi] = z;
    if (rt->color) write_frag(dc, rt->color + pi, m + dc->o_color);
    dc->g->stat_pixels++;
}
#define shade_pixel(dc, x, y, z, w) shade_pixel_t(dc, x, y, z, w, 0)

/* ---------------- triangles ---------------- */
typedef struct SV { float x, y, z, iw; const float *v; } SV;

static void raster_tri(DC *dc, const float *A, const float *B, const float *Cv)
{
    SGL *g = dc->g;
    SV v[3];
    const float *in[3] = { A, B, Cv };
    for (int i = 0; i < 3; i++) {
        float iw = 1.0f / in[i][3];
        v[i].x = dc->vpx + (in[i][0] * iw + 1.0f) * 0.5f * dc->vpw;
        v[i].y = dc->vpy + (in[i][1] * iw + 1.0f) * 0.5f * dc->vph;
        v[i].z = ((dc->zf - dc->zn) * (in[i][2] * iw) + (dc->zf + dc->zn)) * 0.5f;
        v[i].iw = iw;
        v[i].v = in[i] + 4;
    }
    /* snap to 1/8 pixel */
    int X[3], Y[3];
    for (int i = 0; i < 3; i++) {
        float fx = v[i].x * 8.0f, fy = v[i].y * 8.0f;
        X[i] = (int)(fx + (fx < 0 ? -0.5f : 0.5f));
        Y[i] = (int)(fy + (fy < 0 ? -0.5f : 0.5f));
    }
    long long area2 = (long long)(X[1] - X[0]) * (Y[2] - Y[0]) - (long long)(X[2] - X[0]) * (Y[1] - Y[0]);
    if (area2 == 0) return;
    int ccw = area2 > 0;
    int front = (g->front_face == GL_CCW) ? ccw : !ccw;
    if (g->cull) {
        if (g->cull_face == GL_FRONT_AND_BACK) return;
        if (g->cull_face == GL_BACK && !front) return;
        if (g->cull_face == GL_FRONT && front) return;
    }
    if (!ccw) {
        SV t = v[1]; v[1] = v[2]; v[2] = t;
        int tx = X[1]; X[1] = X[2]; X[2] = tx; tx = Y[1]; Y[1] = Y[2]; Y[2] = tx;
        area2 = -area2;
    }
    g->stat_tris++;
    int minX = X[0], maxX = X[0], minY = Y[0], maxY = Y[0];
    for (int i = 1; i < 3; i++) { if (X[i] < minX) minX = X[i]; if (X[i] > maxX) maxX = X[i]; if (Y[i] < minY) minY = Y[i]; if (Y[i] > maxY) maxY = Y[i]; }
    int x0 = minX >> 3, x1 = (maxX + 7) >> 3, y0 = minY >> 3, y1 = (maxY + 7) >> 3;
    if (x0 < dc->cx0) x0 = dc->cx0; if (y0 < dc->cy0) y0 = dc->cy0;
    if (x1 > dc->cx1) x1 = dc->cx1; if (y1 > dc->cy1) y1 = dc->cy1;
    if (x0 >= x1 || y0 >= y1) return;

    float *m = dc->fmem;
    if (dc->o_front >= 0) m[dc->o_front] = front ? 1.0f : 0.0f;
    /* integer edge functions, sampled at pixel centres */
    int EA[3], EB[3], E0[3];
    int px = x0 * 8 + 4, py = y0 * 8 + 4;
    for (int i = 0; i < 3; i++) {
        int p = (i + 1) % 3, q = (i + 2) % 3;
        int dy = Y[q] - Y[p], dx = X[q] - X[p];
        EA[i] = -dy; EB[i] = dx;
        int tl = dy < 0 || (dy == 0 && dx < 0);
        long long e = (long long)dx * (py - Y[p]) - (long long)dy * (px - X[p]);
        E0[i] = (int)e - (tl ? 0 : 1);   /* inside iff E >= 0 */
    }
    /* plane equations: value(x,y) = c + dx*X + dy*Y over pixel indices from (x0,y0) */
    float inva = 1.0f / (float)area2;
    float l0x = EA[0] * 8 * inva, l0y = EB[0] * 8 * inva, l1x = EA[1] * 8 * inva, l1y = EB[1] * 8 * inva;
    float l0c = (E0[0] + 0.5f) * inva, l1c = (E0[1] + 0.5f) * inva;   /* barycentrics at (x0,y0) */
    /* derived planes for any per-vertex value a: a2 + (a0-a2)*l0 + (a1-a2)*l1 */
#define PLANE(a0, a1, a2, C, DX, DY) do { float d0 = (a0) - (a2), d1 = (a1) - (a2); \
        C = (a2) + d0 * l0c + d1 * l1c; DX = d0 * l0x + d1 * l1x; DY = d0 * l0y + d1 * l1y; } while (0)
    float zc, zdx, zdy, qc, qdx, qdy;
    PLANE(v[0].z, v[1].z, v[2].z, zc, zdx, zdy);
    PLANE(v[0].iw, v[1].iw, v[2].iw, qc, qdx, qdy);
    int nv = dc->nv;
    float mn = v[0].iw, mx = v[0].iw;
    for (int i = 1; i < 3; i++) { if (v[i].iw < mn) mn = v[i].iw; if (v[i].iw > mx) mx = v[i].iw; }
    int affine = (mx - mn) <= mx * 0.002f;
    float vc[MAXV], vdx[MAXV], vdy[MAXV];
    for (int k = 0; k < nv; k++) {
        float a0 = v[0].v[k], a1 = v[1].v[k], a2 = v[2].v[k];
        if (!affine) { a0 *= v[0].iw; a1 *= v[1].iw; a2 *= v[2].iw; }
        PLANE(a0, a1, a2, vc[k], vdx[k], vdy[k]);
    }
#undef PLANE
    const int *vfs = dc->vfs;
    RT *rt = &dc->rt;
    int dt = dc->depth_test;
    for (int y = y0; y < y1; y++) {
        int ry = y - y0;
        int e0 = E0[0] + EB[0] * 8 * ry, e1 = E0[1] + EB[1] * 8 * ry, e2 = E0[2] + EB[2] * 8 * ry;
        int sa0 = EA[0] * 8, sa1 = EA[1] * 8, sa2 = EA[2] * 8;
        int x = x0;
        /* find span start */
        while (x < x1 && (e0 | e1 | e2) < 0) { e0 += sa0; e1 += sa1; e2 += sa2; x++; }
        if (x >= x1) continue;
        float fx = (float)(x - x0), fy = (float)ry;
        float z = zc + zdx * fx + zdy * fy, q = qc + qdx * fx + qdy * fy;
        float va[MAXV];
        for (int k = 0; k < nv; k++) va[k] = vc[k] + vdx[k] * fx + vdy[k] * fy;
        float *drow = rt->depth ? rt->depth + (size_t)y * rt->w : NULL;
        while (x < x1 && (e0 | e1 | e2) >= 0) {
            if (!dt || depth_pass(dc->depth_func, z, drow[x])) {
                if (affine) for (int k = 0; k < nv; k++) m[vfs[k]] = va[k];
                else { float w = 1.0f / q; for (int k = 0; k < nv; k++) m[vfs[k]] = va[k] * w; }
                shade_pixel_t(dc, x, y, z, q, 1);
            }
            e0 += sa0; e1 += sa1; e2 += sa2; x++;
            z += zdx; q += qdx;
            for (int k = 0; k < nv; k++) va[k] += vdx[k];
        }
    }
}

/* clip polygon against plane dot(p, (a,b,c,d)) >= 0 in clip space */
static int clip_poly(DC *dc, float *in, int n, float *out, const float *pl)
{
    int st = dc->vstride, no = 0;
    for (int i = 0; i < n; i++) {
        const float *P = in + i * st, *Q = in + ((i + 1) % n) * st;
        float dp = pl[0] * P[0] + pl[1] * P[1] + pl[2] * P[2] + pl[3] * P[3];
        float dq = pl[0] * Q[0] + pl[1] * Q[1] + pl[2] * Q[2] + pl[3] * Q[3];
        if (dp >= 0) { memcpy(out + no * st, P, st * sizeof(float)); no++; }
        if ((dp >= 0) != (dq >= 0)) {
            float t = dp / (dp - dq);
            float *O = out + no * st;
            for (int k = 0; k < st; k++) O[k] = P[k] + (Q[k] - P[k]) * t;
            no++;
        }
    }
    return no;
}

#define GB 2.0f
static const float planes[7][4] = {
    { 0, 0, 1, 1 }, { 0, 0, -1, 1 }, { 0, 0, 0, 1 },
    { 1, 0, 0, GB }, { -1, 0, 0, GB }, { 0, 1, 0, GB }, { 0, -1, 0, GB } };

static void draw_tri(DC *dc, const float *A, const float *B, const float *C)
{
    const float *vs[3] = { A, B, C };
    int oc[3], mask = 0, all = 0x7F;
    for (int i = 0; i < 3; i++) {
        const float *p = vs[i];
        float x = p[0], y = p[1], z = p[2], w = p[3], w2 = w + w;
        int c = 0;
        if (z < -w) c |= 1;
        if (z > w) c |= 2;
        if (w < 1e-5f) c |= 4;
        if (x < -w2) c |= 8;
        if (x > w2) c |= 16;
        if (y < -w2) c |= 32;
        if (y > w2) c |= 64;
        oc[i] = c; mask |= c; all &= c;
    }
    if (!mask) { raster_tri(dc, A, B, C); return; }
    if (all) return;
    int st = dc->vstride;
    float bufA[16 * (4 + MAXV)], bufB[16 * (4 + MAXV)];
    memcpy(bufA, A, st * 4); memcpy(bufA + st, B, st * 4); memcpy(bufA + 2 * st, C, st * 4);
    int n = 3;
    float *cur = bufA, *oth = bufB;
    for (int k = 0; k < 7 && n >= 3; k++) {
        if (!(mask & (1 << k))) continue;
        float pl[4] = { planes[k][0], planes[k][1], planes[k][2], planes[k][3] };
        if (k == 2) pl[3] = 1.0f;
        if (k == 2) {   /* w >= eps */
            int no = 0;
            for (int i = 0; i < n; i++) {
                const float *P = cur + i * st, *Q = cur + ((i + 1) % n) * st;
                float dp = P[3] - 1e-5f, dq = Q[3] - 1e-5f;
                if (dp >= 0) { memcpy(oth + no * st, P, st * 4); no++; }
                if ((dp >= 0) != (dq >= 0)) { float t = dp / (dp - dq); float *O = oth + no * st; for (int q = 0; q < st; q++) O[q] = P[q] + (Q[q] - P[q]) * t; no++; }
            }
            n = no;
        } else n = clip_poly(dc, cur, n, oth, pl);
        float *t = cur; cur = oth; oth = t;
        if (n > 14) n = 14;
    }
    for (int i = 1; i + 1 < n; i++) raster_tri(dc, cur, cur + i * st, cur + (i + 1) * st);
}

/* ---------------- lines & points ---------------- */
static void to_screen(DC *dc, const float *p, float *sx, float *sy, float *sz, float *iw)
{
    *iw = 1.0f / p[3];
    *sx = dc->vpx + (p[0] * *iw + 1) * 0.5f * dc->vpw;
    *sy = dc->vpy + (p[1] * *iw + 1) * 0.5f * dc->vph;
    *sz = ((dc->zf - dc->zn) * p[2] * *iw + (dc->zf + dc->zn)) * 0.5f;
}

static void draw_line(DC *dc, const float *A, const float *B)
{
    int st = dc->vstride;
    float a[4 + MAXV], b[4 + MAXV];
    memcpy(a, A, st * 4); memcpy(b, B, st * 4);
    /* clip against near/far and w>0 */
    for (int k = 0; k < 3; k++) {
        float da, db;
        if (k == 0) { da = a[2] + a[3]; db = b[2] + b[3]; }
        else if (k == 1) { da = a[3] - a[2]; db = b[3] - b[2]; }
        else { da = a[3] - 1e-5f; db = b[3] - 1e-5f; }
        if (da < 0 && db < 0) return;
        if (da < 0) { float t = da / (da - db); for (int i = 0; i < st; i++) a[i] += (b[i] - a[i]) * t; }
        else if (db < 0) { float t = db / (db - da); for (int i = 0; i < st; i++) b[i] += (a[i] - b[i]) * t; }
    }
    float ax, ay, az, aw, bx, by, bz, bw;
    to_screen(dc, a, &ax, &ay, &az, &aw);
    to_screen(dc, b, &bx, &by, &bz, &bw);
    float dx = bx - ax, dy = by - ay;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (steps < 1) steps = 1;
    if (steps > 8192) steps = 8192;
    float *m = dc->fmem;
    if (dc->o_front >= 0) m[dc->o_front] = 1;
    int lw = (int)(dc->g->line_width * dc->rt.sx + 0.5f); if (lw < 1) lw = 1; if (lw > 8) lw = 8;
    for (int s = 0; s < steps; s++) {
        float t = (s + 0.5f) / steps;
        float x = ax + dx * t, y = ay + dy * t;
        int ix = (int)floorf(x), iy = (int)floorf(y);
        float z = az + (bz - az) * t;
        float q = aw + (bw - aw) * t;
        float wa = (1 - t) * aw / q, wb = t * bw / q;
        for (int k = 0; k < dc->nv; k++) m[dc->vfs[k]] = a[4 + k] * wa + b[4 + k] * wb;
        for (int oy = 0; oy < lw; oy++) for (int ox = 0; ox < lw; ox++) {
            int px = ix + ox - lw / 2, py = iy + oy - lw / 2;
            if (px < dc->cx0 || py < dc->cy0 || px >= dc->cx1 || py >= dc->cy1) continue;
            shade_pixel(dc, px, py, z, q);
        }
    }
}

static void draw_point(DC *dc, const float *P, float size)
{
    if (P[3] <= 0 || P[2] < -P[3] || P[2] > P[3]) return;
    float x, y, z, iw;
    to_screen(dc, P, &x, &y, &z, &iw);
    size *= dc->rt.sx;
    if (size < 1) size = 1; if (size > 128) size = 128;
    float h = size * 0.5f;
    int x0 = (int)floorf(x - h + 0.5f), x1 = (int)floorf(x + h + 0.5f), y0 = (int)floorf(y - h + 0.5f), y1 = (int)floorf(y + h + 0.5f);
    if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
    float *m = dc->fmem;
    if (dc->o_front >= 0) m[dc->o_front] = 1;
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        if (px < dc->cx0 || py < dc->cy0 || px >= dc->cx1 || py >= dc->cy1) continue;
        for (int k = 0; k < dc->nv; k++) m[dc->vfs[k]] = P[4 + k];
        if (dc->o_pcoord >= 0) {
            m[dc->o_pcoord] = 0.5f + (px + 0.5f - x) / size;
            m[dc->o_pcoord + 1] = 0.5f - (py + 0.5f - y) / size;
        }
        shade_pixel(dc, px, py, z, iw);
    }
}

/* ---------------- draw calls ---------------- */
static float *vert(DC *dc, float *cache, uint8_t *done, int base, int idx)
{
    float *o = cache + (size_t)(idx - base) * dc->vstride;
    if (!done[idx - base]) { run_vs(dc, idx, o); done[idx - base] = 1; }
    return o;
}

static void assemble(DC *dc, int mode, int count, const uint32_t *ids, int first, int base, int range)
{
    SGL *g = dc->g;
    size_t need = (size_t)range * dc->vstride;
    if (need > (size_t)g->vcache_cap) { free(g->vcache); g->vcache = malloc(need * sizeof(float)); g->vcache_cap = need; }
    if (range > g->vdone_cap) { free(g->vdone); g->vdone = malloc(range); g->vdone_cap = range; }
    if (!g->vcache || !g->vdone) { g->vcache_cap = g->vdone_cap = 0; return; }
    memset(g->vdone, 0, range);
    float *C = g->vcache; uint8_t *D = g->vdone;
#define IDX(i) (ids ? (int)ids[i] : first + (i))
#define V(i) vert(dc, C, D, base, IDX(i))
    switch (mode) {
    case GL_TRIANGLES:
        for (int i = 0; i + 2 < count; i += 3) { float *a = V(i), *b = V(i + 1), *c = V(i + 2); draw_tri(dc, a, b, c); }
        break;
    case GL_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < count; i++) {
            float *a = V(i), *b = V(i + 1), *c = V(i + 2);
            if (i & 1) draw_tri(dc, b, a, c); else draw_tri(dc, a, b, c);
        }
        break;
    case GL_TRIANGLE_FAN:
        for (int i = 1; i + 1 < count; i++) { float *a = V(0), *b = V(i), *c = V(i + 1); draw_tri(dc, a, b, c); }
        break;
    case GL_LINES:
        for (int i = 0; i + 1 < count; i += 2) { float *a = V(i), *b = V(i + 1); draw_line(dc, a, b); }
        break;
    case GL_LINE_STRIP: case GL_LINE_LOOP:
        for (int i = 0; i + 1 < count; i++) { float *a = V(i), *b = V(i + 1); draw_line(dc, a, b); }
        if (mode == GL_LINE_LOOP && count > 2) { float *a = V(count - 1), *b = V(0); draw_line(dc, a, b); }
        break;
    case GL_POINTS: {
        for (int i = 0; i < count; i++) {
            float *a = V(i);
            float ps = 1.0f;
            if (dc->vs->writes_pointsize) {
                /* point size is not cached per vertex; re-run to read it */
                run_vs(dc, IDX(i), a);
                ps = dc->vmem[dc->vs->off_pointsize];
            }
            draw_point(dc, a, ps);
        }
        break; }
    default: sgl_set_error(g, GL_INVALID_ENUM);
    }
#undef V
#undef IDX
}

void sgl_draw_arrays(SGL *g, int mode, int first, int count)
{
    if (first < 0 || count < 0) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    if (!count) return;
    DC dc;
    if (!prep_draw(g, &dc)) return;
    assemble(&dc, mode, count, NULL, first, first, count);
}

void sgl_draw_elements(SGL *g, int mode, int count, int type, int offset)
{
    if (count < 0 || offset < 0) { sgl_set_error(g, GL_INVALID_VALUE); return; }
    SGLBuffer *eb = sgl_obj(&g->buffers, g->element_buffer);
    if (!eb) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    int isz = type == GL_UNSIGNED_BYTE ? 1 : type == GL_UNSIGNED_SHORT ? 2 : type == GL_UNSIGNED_INT ? 4 : 0;
    if (!isz) { sgl_set_error(g, GL_INVALID_ENUM); return; }
    if (offset + (long)count * isz > eb->size) { sgl_set_error(g, GL_INVALID_OPERATION); return; }
    if (!count) return;
    DC dc;
    if (!prep_draw(g, &dc)) return;
    uint32_t *ids = malloc(sizeof(uint32_t) * count);
    if (!ids) return;
    const uint8_t *p = eb->data + offset;
    uint32_t mn = 0xFFFFFFFFu, mx = 0;
    for (int i = 0; i < count; i++) {
        uint32_t v = isz == 1 ? p[i] : isz == 2 ? (uint32_t)(p[2 * i] | (p[2 * i + 1] << 8)) :
                     (uint32_t)(p[4 * i] | (p[4 * i + 1] << 8) | (p[4 * i + 2] << 16) | ((uint32_t)p[4 * i + 3] << 24));
        ids[i] = v;
        if (v < mn) mn = v; if (v > mx) mx = v;
    }
    if (mx - mn > 1000000) { free(ids); sgl_set_error(g, GL_INVALID_OPERATION); return; }
    assemble(&dc, mode, count, ids, 0, (int)mn, (int)(mx - mn + 1));
    free(ids);
}
