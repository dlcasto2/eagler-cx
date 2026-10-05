/* SGL: software WebGL 1.0 implementation (state machine + rasterizer) */
#ifndef SGL_H
#define SGL_H
#include <stdint.h>
#include "glsl.h"

#define SGL_MAX_ATTRIBS 16
#define SGL_MAX_UNITS 8

typedef struct SGLBuffer { uint8_t *data; int size; int usage; } SGLBuffer;

typedef struct SGLTexture {
    int target;              /* 0 until bound; GL_TEXTURE_2D / CUBE_MAP */
    int w, h;
    uint32_t *px;            /* RGBA8 level 0 (2D) */
    uint32_t *face[6];       /* cube faces */
    int fw, fh;
    int minf, magf, wraps, wrapt;
} SGLTexture;

typedef struct SGLRenderbuffer { int w, h, format; float *depth; uint32_t *color; } SGLRenderbuffer;

typedef struct SGLFramebuffer {
    int color_tex, color_tex_face, color_rb, depth_rb, stencil_rb;
} SGLFramebuffer;

typedef struct SGLShader { int type; char *src; int compiled, compile_ok; char *log; int deleted; } SGLShader;

typedef struct SGLUniform {
    char name[48];
    GType type;
    int vs_off, fs_off;
    int esize;     /* floats per element */
    int count;     /* elements (1 if not array) */
    int gltype;    /* GL type enum */
} SGLUniform;

typedef struct SGLAttrib { char name[48]; GType type; int vs_off; int loc; int gltype; } SGLAttrib;

typedef struct SGLProgram {
    int vs, fs;               /* attached shader ids */
    char *bind_names[SGL_MAX_ATTRIBS];
    int linked, link_ok;
    char *log;
    GShader *v, *f;
    float *vmem, *fmem;
    SGLAttrib *attrs; int nattrs;
    SGLUniform *unis; int nunis;
    int nvary;                /* floats */
    int *vary_vs, *vary_fs;   /* per float component offsets */
    int deleted;
} SGLProgram;

typedef struct SGLAttribState {
    int enabled, buffer, size, type, normalized, stride, offset;
    float generic[4];
} SGLAttribState;

typedef struct SGLObjTab { void **v; int n, cap; } SGLObjTab;

typedef struct SGL {
    /* default framebuffer (drawing buffer) */
    int w, h;                 /* real buffer size */
    int vw, vh;               /* virtual (canvas) size */
    float sx, sy;             /* real/virtual */
    uint32_t *color;
    float *depth;
    int has_depth, has_alpha;

    float clear_color[4], clear_depth; int clear_stencil;
    int viewport[4], scissor[4];
    int scissor_test, depth_test, cull, blend, stencil_test, dither, poly_offset;
    int depth_func, depth_mask, cull_face, front_face;
    int bsrc_rgb, bdst_rgb, bsrc_a, bdst_a, beq_rgb, beq_a;
    float blend_color[4];
    int color_mask[4];
    float depth_range[2];
    float line_width;
    float po_factor, po_units;
    int stencil_func, stencil_ref, stencil_mask, stencil_wmask;

    int array_buffer, element_buffer, program, active_unit;
    int tex2d[SGL_MAX_UNITS], texcube[SGL_MAX_UNITS];
    int framebuffer, renderbuffer;
    SGLAttribState attr[SGL_MAX_ATTRIBS];
    int unpack_flip_y, unpack_premul, unpack_align, pack_align;
    int error;

    SGLObjTab buffers, textures, shaders, programs, framebuffers, renderbuffers;

    /* per-draw scratch */
    float *vcache; uint8_t *vdone; int vcache_cap, vdone_cap;
    long long stat_pixels, stat_tris;
} SGL;

/* GL enums used internally */
enum {
    GL_NO_ERROR = 0, GL_INVALID_ENUM = 0x500, GL_INVALID_VALUE = 0x501, GL_INVALID_OPERATION = 0x502,
    GL_POINTS = 0, GL_LINES = 1, GL_LINE_LOOP = 2, GL_LINE_STRIP = 3, GL_TRIANGLES = 4, GL_TRIANGLE_STRIP = 5, GL_TRIANGLE_FAN = 6,
    GL_DEPTH_BUFFER_BIT = 0x100, GL_STENCIL_BUFFER_BIT = 0x400, GL_COLOR_BUFFER_BIT = 0x4000,
    GL_ZERO = 0, GL_ONE = 1, GL_SRC_COLOR = 0x300, GL_ONE_MINUS_SRC_COLOR = 0x301, GL_SRC_ALPHA = 0x302,
    GL_ONE_MINUS_SRC_ALPHA = 0x303, GL_DST_ALPHA = 0x304, GL_ONE_MINUS_DST_ALPHA = 0x305, GL_DST_COLOR = 0x306,
    GL_ONE_MINUS_DST_COLOR = 0x307, GL_SRC_ALPHA_SATURATE = 0x308,
    GL_CONSTANT_COLOR = 0x8001, GL_ONE_MINUS_CONSTANT_COLOR = 0x8002, GL_CONSTANT_ALPHA = 0x8003, GL_ONE_MINUS_CONSTANT_ALPHA = 0x8004,
    GL_FUNC_ADD = 0x8006, GL_FUNC_SUBTRACT = 0x800A, GL_FUNC_REVERSE_SUBTRACT = 0x800B,
    GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893,
    GL_FRONT = 0x404, GL_BACK = 0x405, GL_FRONT_AND_BACK = 0x408,
    GL_CULL_FACE = 0xB44, GL_BLEND = 0xBE2, GL_DITHER = 0xBD0, GL_STENCIL_TEST = 0xB90, GL_DEPTH_TEST = 0xB71,
    GL_SCISSOR_TEST = 0xC11, GL_POLYGON_OFFSET_FILL = 0x8037,
    GL_CW = 0x900, GL_CCW = 0x901,
    GL_BYTE = 0x1400, GL_UNSIGNED_BYTE = 0x1401, GL_SHORT = 0x1402, GL_UNSIGNED_SHORT = 0x1403, GL_INT = 0x1404,
    GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406, GL_HALF_FLOAT_OES = 0x8D61,
    GL_DEPTH_COMPONENT = 0x1902, GL_ALPHA = 0x1906, GL_RGB = 0x1907, GL_RGBA = 0x1908, GL_LUMINANCE = 0x1909, GL_LUMINANCE_ALPHA = 0x190A,
    GL_UNSIGNED_SHORT_4_4_4_4 = 0x8033, GL_UNSIGNED_SHORT_5_5_5_1 = 0x8034, GL_UNSIGNED_SHORT_5_6_5 = 0x8363,
    GL_FRAGMENT_SHADER = 0x8B30, GL_VERTEX_SHADER = 0x8B31,
    GL_NEVER = 0x200, GL_LESS = 0x201, GL_EQUAL = 0x202, GL_LEQUAL = 0x203, GL_GREATER = 0x204, GL_NOTEQUAL = 0x205,
    GL_GEQUAL = 0x206, GL_ALWAYS = 0x207,
    GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_NEAREST_MIPMAP_NEAREST = 0x2700, GL_LINEAR_MIPMAP_NEAREST = 0x2701,
    GL_NEAREST_MIPMAP_LINEAR = 0x2702, GL_LINEAR_MIPMAP_LINEAR = 0x2703,
    GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803,
    GL_TEXTURE_2D = 0xDE1, GL_TEXTURE_CUBE_MAP = 0x8513, GL_TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515,
    GL_TEXTURE0 = 0x84C0, GL_REPEAT = 0x2901, GL_CLAMP_TO_EDGE = 0x812F, GL_MIRRORED_REPEAT = 0x8370,
    GL_FRAMEBUFFER = 0x8D40, GL_RENDERBUFFER = 0x8D41, GL_COLOR_ATTACHMENT0 = 0x8CE0, GL_DEPTH_ATTACHMENT = 0x8D00,
    GL_STENCIL_ATTACHMENT = 0x8D20, GL_DEPTH_STENCIL_ATTACHMENT = 0x821A,
    GL_RGBA4 = 0x8056, GL_RGB5_A1 = 0x8057, GL_RGB565 = 0x8D62, GL_DEPTH_COMPONENT16 = 0x81A5, GL_STENCIL_INDEX8 = 0x8D48, GL_DEPTH_STENCIL = 0x84F9,
    GL_FRAMEBUFFER_COMPLETE = 0x8CD5, GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6, GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7,
    GL_UNPACK_ALIGNMENT = 0xCF5, GL_PACK_ALIGNMENT = 0xD05, GL_UNPACK_FLIP_Y_WEBGL = 0x9240, GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL = 0x9241,
    GL_FLOAT_VEC2 = 0x8B50, GL_FLOAT_VEC3 = 0x8B51, GL_FLOAT_VEC4 = 0x8B52, GL_INT_VEC2 = 0x8B53, GL_INT_VEC3 = 0x8B54,
    GL_INT_VEC4 = 0x8B55, GL_BOOL = 0x8B56, GL_BOOL_VEC2 = 0x8B57, GL_BOOL_VEC3 = 0x8B58, GL_BOOL_VEC4 = 0x8B59,
    GL_FLOAT_MAT2 = 0x8B5A, GL_FLOAT_MAT3 = 0x8B5B, GL_FLOAT_MAT4 = 0x8B5C, GL_SAMPLER_2D = 0x8B5E, GL_SAMPLER_CUBE = 0x8B60
};

SGL *sgl_create(int vw, int vh, int rw, int rh, int depth, int alpha);
void sgl_resize(SGL *g, int vw, int vh, int rw, int rh);
void sgl_destroy(SGL *g);

/* objects: return id > 0 */
int sgl_create_obj(SGL *g, SGLObjTab *t, void *p);
void *sgl_obj(SGLObjTab *t, int id);

int sgl_create_buffer(SGL *g);
void sgl_delete_buffer(SGL *g, int id);
void sgl_bind_buffer(SGL *g, int target, int id);
void sgl_buffer_data(SGL *g, int target, const void *data, int size, int usage);
void sgl_buffer_sub_data(SGL *g, int target, int offset, const void *data, int size);

int sgl_create_texture(SGL *g);
void sgl_delete_texture(SGL *g, int id);
void sgl_bind_texture(SGL *g, int target, int id);
void sgl_tex_parameter(SGL *g, int target, int pname, int v);
/* tex image from raw pixel data in GL format/type (data may be NULL) */
void sgl_tex_image(SGL *g, int target, int level, int ifmt, int w, int h, int fmt, int type, const void *data);
void sgl_tex_sub_image(SGL *g, int target, int level, int x, int y, int w, int h, int fmt, int type, const void *data);
/* tex image from RGBA8 top-down image (HTML image element semantics) */
void sgl_tex_image_rgba(SGL *g, int target, int level, int w, int h, const uint8_t *rgba, int fmt);
void sgl_copy_tex_image(SGL *g, int target, int level, int x, int y, int w, int h);
void sgl_generate_mipmap(SGL *g, int target);

int sgl_create_shader(SGL *g, int type);
void sgl_shader_source(SGL *g, int id, const char *src);
void sgl_compile_shader(SGL *g, int id);
void sgl_delete_shader(SGL *g, int id);
int sgl_create_program(SGL *g);
void sgl_attach_shader(SGL *g, int prog, int sh);
void sgl_detach_shader(SGL *g, int prog, int sh);
void sgl_bind_attrib_location(SGL *g, int prog, int loc, const char *name);
void sgl_link_program(SGL *g, int prog);
void sgl_use_program(SGL *g, int prog);
void sgl_delete_program(SGL *g, int prog);
int sgl_get_attrib_location(SGL *g, int prog, const char *name);
/* returns uniform index, elem via *elem; -1 if none */
int sgl_get_uniform_location(SGL *g, int prog, const char *name, int *elem);
void sgl_uniform(SGL *g, int prog, int uidx, int elem, const float *v, int nfloats);
void sgl_get_uniform(SGL *g, int prog, int uidx, int elem, float *out, int *n);

void sgl_vertex_attrib_pointer(SGL *g, int idx, int size, int type, int norm, int stride, int offset);
void sgl_enable_attrib(SGL *g, int idx, int en);
void sgl_vertex_attrib(SGL *g, int idx, const float *v, int n);

void sgl_enable(SGL *g, int cap, int en);
int sgl_is_enabled(SGL *g, int cap);
void sgl_clear(SGL *g, int mask);
void sgl_viewport(SGL *g, int x, int y, int w, int h);
void sgl_scissor(SGL *g, int x, int y, int w, int h);
void sgl_draw_arrays(SGL *g, int mode, int first, int count);
void sgl_draw_elements(SGL *g, int mode, int count, int type, int offset);
void sgl_read_pixels(SGL *g, int x, int y, int w, int h, uint8_t *out);

int sgl_create_framebuffer(SGL *g);
void sgl_delete_framebuffer(SGL *g, int id);
void sgl_bind_framebuffer(SGL *g, int id);
void sgl_framebuffer_texture(SGL *g, int attach, int textarget, int tex);
void sgl_framebuffer_renderbuffer(SGL *g, int attach, int rb);
int sgl_check_framebuffer(SGL *g);
int sgl_create_renderbuffer(SGL *g);
void sgl_delete_renderbuffer(SGL *g, int id);
void sgl_bind_renderbuffer(SGL *g, int id);
void sgl_renderbuffer_storage(SGL *g, int fmt, int w, int h);

void sgl_set_error(SGL *g, int e);
#endif
