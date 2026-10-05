#ifndef PAGE_H
#define PAGE_H
#include "../js/qjs/quickjs.h"
#include <stdint.h>

enum { DI_RECT, DI_TEXT, DI_CANVAS, DI_IMAGE, DI_HR, DI_BORDER, DI_BULLET };

typedef struct DItem {
    uint8_t type, font, fixed, link_hl;
    int16_t link;          /* link index or -1 */
    int x, y, w, h;
    uint32_t color;        /* ARGB */
    char *text; int len;
    JSValue obj;           /* canvas/img element */
} DItem;

typedef struct Link { JSValue el; int x, y, w, h; int fixed; } Link;

#define LOG_LINES 48
typedef struct Page {
    JSRuntime *rt;
    JSContext *ctx;
    char path[512];        /* resolved file path of the page */
    char dir[512];
    char title[128];
    int dirty;             /* DOM changed -> relayout */
    int redraw;
    int laid_out;
    /* layout result */
    DItem *items; int nitems, capitems;
    Link *links; int nlinks, caplinks;
    int doc_h;
    uint32_t bg;
    int focus_link;
    int scroll_y;
    /* requests */
    char nav[512]; int nav_req;
    /* console */
    char log[LOG_LINES][160]; uint8_t loglvl[LOG_LINES]; int logn, loghead;
    int errors;
    int fullscreen_canvas;
    uint32_t deadline;     /* interrupt deadline (ms) */
    int interrupted;
} Page;

Page *page_open(const char *path);
void page_close(Page *pg);
int page_tick(Page *pg, uint32_t now);     /* runs timers + rAF; returns 1 if animation pending */
void page_layout(Page *pg);
void page_key(Page *pg, int down, const char *key, const char *code, int keyCode, int shift, int repeat);
void page_mouse(Page *pg, const char *type, int x, int y, JSValue el, int dx, int dy, int buttons);
int page_has_keys(Page *pg);
int page_has_mouse(Page *pg);
void page_log(Page *pg, int lvl, const char *fmt, ...);
int page_resolve(Page *pg, const char *url, char *out, int outsz);
char *load_file(const char *path, int *len);
void page_run_jobs(Page *pg);
JSValue page_element_at(Page *pg, int x, int y);   /* page coords */

/* provided by browser */
void ui_alert(const char *msg);
#endif
