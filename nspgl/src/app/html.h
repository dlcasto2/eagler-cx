#ifndef HTML_H
#define HTML_H
#include "../js/qjs/quickjs.h"
/* parse HTML into nested JS arrays (see html.c) */
JSValue html_parse(JSContext *ctx, const char *src, int len);
#endif
