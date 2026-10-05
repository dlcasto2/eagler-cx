#ifndef CSS_H
#define CSS_H
#include <stdint.h>
#define SCREEN_W_CSS 320
#define SCREEN_H_CSS 240
int css_color(const char *s, uint32_t *argb);
float css_len(const char *s, float pct_of, float dflt);
#endif
