#ifndef CANVAS2D_H
#define CANVAS2D_H
#include "qjs/quickjs.h"
#include <stdint.h>
void canvas2d_init(JSContext *ctx);
void canvas2d_free_protos(JSContext *ctx);
JSValue canvas2d_create(JSContext *ctx, JSValueConst canvas);
/* pixels of a 2D or WebGL context: RGBA8, flip=1 means bottom-up rows */
int canvas_ctx_pixels(JSContext *ctx, JSValueConst ctxobj, const uint32_t **px, int *w, int *h, int *flip);
int canvas2d_is(JSValueConst ctxobj);
#endif
