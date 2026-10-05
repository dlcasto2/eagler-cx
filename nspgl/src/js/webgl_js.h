#ifndef WEBGL_JS_H
#define WEBGL_JS_H
#include "qjs/quickjs.h"
#include <stdint.h>
#include "../gl/sgl.h"
#define SCR_MAXW 320
#define SCR_MAXH 240
void webgl_init(JSContext *ctx);
void webgl_free_protos(JSContext *ctx);
JSValue webgl_create(JSContext *ctx, JSValueConst canvas, JSValueConst attrs);
int webgl_pixels(JSContext *ctx, JSValueConst ctxobj, const uint32_t **px, int *w, int *h, int *alpha);
SGL *webgl_sgl(JSValueConst ctxobj);
void webgl_set_quality(int pct);
#endif
