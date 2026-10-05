#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>

#define SCR_W 320
#define SCR_H 240

enum {
    K_NONE, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESC, K_TAB, K_MENU, K_DEL, K_SPACE,
    K_SHIFT, K_CTRL, K_DOC, K_CLICK, K_PLUS, K_MINUS, K_MUL, K_DIV, K_COMMA, K_PERIOD,
    K_HOME, K_EQU, K_LP, K_RP, K_VAR, K_CAT, K_SCRATCH, K_EE, K_NEG, K_RET, K_TRIG, K_POW, K_SQU,
    K_A, K_Z = K_A + 25, K_0, K_9 = K_0 + 9,
    K_COUNT
};

int plat_init(int argc, char **argv);
void plat_quit(void);
uint16_t *plat_fb(void);           /* SCR_W*SCR_H RGB565, row-major top-down */
void plat_present(void);
uint32_t plat_ms(void);
void plat_wait_ms(int ms);
/* sample key state; fills down[K_COUNT] */
void plat_keys(uint8_t *down);
/* touchpad: returns 1 if touched; dx/dy movement since last call (pixels) */
int plat_touch(int *dx, int *dy);
int plat_on_pressed(void);         /* abort key (ON) */
const char *plat_start_dir(void);
const char *plat_arg_file(void);   /* file passed on command line or NULL */
int plat_is_host(void);
void plat_log(const char *s);
/* run fn on a large private stack (calculator) */
void plat_run(void (*fn)(void));
#endif
