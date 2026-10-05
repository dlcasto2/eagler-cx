/* TI-Nspire (Ndless) platform layer */
#ifdef _TINSPIRE
#include "platform.h"
#include <libndls.h>
#include <keys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int64_t nspire_time_ms(void);
static uint16_t *fb;
static char start_dir[256];
static char arg_file[256];
static touchpad_report_t tp_last;
static int tp_had;

/* SP804 timer (second counter of the first timer module), 32768 Hz */
static volatile uint32_t *const timer_load = (volatile uint32_t *)0x900C0020;
static volatile uint32_t *const timer_value = (volatile uint32_t *)0x900C0024;
static volatile uint32_t *const timer_control = (volatile uint32_t *)0x900C0028;
static uint32_t saved_load, saved_control;
static uint32_t last_ticks;
static unsigned old_speed;

/* malloc_usable_size is referenced by QuickJS default allocator */
size_t malloc_usable_size(void *p) { (void)p; return 0; }

int plat_init(int argc, char **argv)
{
    if (argc > 0 && argv[0]) {
        snprintf(start_dir, sizeof start_dir, "%s", argv[0]);
        char *s = strrchr(start_dir, '/');
        if (s) s[1] = 0; else strcpy(start_dir, "/documents/");
    } else strcpy(start_dir, "/documents/");
    if (argc > 1 && argv[1]) snprintf(arg_file, sizeof arg_file, "%s", argv[1]);
    cfg_register_fileext("html", "nspgl");
    cfg_register_fileext("htm", "nspgl");
    fb = calloc(SCR_W * SCR_H, 2);
    if (!fb) return -1;
    old_speed = set_cpu_speed(CPU_SPEED_150MHZ);
    if (!lcd_init(SCR_320x240_565)) return -1;
    saved_load = *timer_load;
    saved_control = *timer_control;
    *timer_control = 0;
    *timer_load = 0xFFFFFFFF;
    *timer_control = 0x82;
    last_ticks = 0;
    nspire_time_ms();
    return 0;
}

void plat_quit(void)
{
    *timer_control = 0;
    *timer_load = saved_load;
    *timer_control = saved_control;
    if (old_speed) set_cpu_speed(old_speed);
    lcd_init(SCR_TYPE_INVALID);
    free(fb);
}

uint16_t *plat_fb(void) { return fb; }
void plat_present(void) { lcd_blit(fb, SCR_320x240_565); }

uint32_t plat_ms(void)
{
    uint32_t t = 0xFFFFFFFFu - *timer_value;
    /* 32768 Hz -> ms with 64-bit math to avoid drift */
    return (uint32_t)(((uint64_t)t * 1000ull) >> 15);
}
/* wall clock in ms for Date.now(): RTC seconds at start + fine timer */
static int64_t t0_ms; static uint32_t t0_ticks;
int64_t nspire_time_ms(void)
{
    if (!t0_ms) { t0_ms = (int64_t)(*(volatile uint32_t *)0x90090000) * 1000; t0_ticks = plat_ms(); if (!t0_ms) t0_ms = 1; }
    return t0_ms + (uint32_t)(plat_ms() - t0_ticks);
}
void plat_wait_ms(int ms) { if (ms > 0) msleep(ms); }

void plat_keys(uint8_t *d)
{
    memset(d, 0, K_COUNT);
    d[K_UP] = isKeyPressed(KEY_NSPIRE_UP) || isKeyPressed(KEY_NSPIRE_LEFTUP) || isKeyPressed(KEY_NSPIRE_UPRIGHT);
    d[K_DOWN] = isKeyPressed(KEY_NSPIRE_DOWN) || isKeyPressed(KEY_NSPIRE_RIGHTDOWN) || isKeyPressed(KEY_NSPIRE_DOWNLEFT);
    d[K_LEFT] = isKeyPressed(KEY_NSPIRE_LEFT) || isKeyPressed(KEY_NSPIRE_LEFTUP) || isKeyPressed(KEY_NSPIRE_DOWNLEFT);
    d[K_RIGHT] = isKeyPressed(KEY_NSPIRE_RIGHT) || isKeyPressed(KEY_NSPIRE_UPRIGHT) || isKeyPressed(KEY_NSPIRE_RIGHTDOWN);
    d[K_ENTER] = isKeyPressed(KEY_NSPIRE_ENTER);
    d[K_RET] = isKeyPressed(KEY_NSPIRE_RET);
    d[K_ESC] = isKeyPressed(KEY_NSPIRE_ESC);
    d[K_TAB] = isKeyPressed(KEY_NSPIRE_TAB);
    d[K_MENU] = isKeyPressed(KEY_NSPIRE_MENU);
    d[K_DEL] = isKeyPressed(KEY_NSPIRE_DEL);
    d[K_SPACE] = isKeyPressed(KEY_NSPIRE_SPACE);
    d[K_SHIFT] = isKeyPressed(KEY_NSPIRE_SHIFT);
    d[K_CTRL] = isKeyPressed(KEY_NSPIRE_CTRL);
    d[K_DOC] = isKeyPressed(KEY_NSPIRE_DOC);
    d[K_CLICK] = isKeyPressed(KEY_NSPIRE_CLICK);
    d[K_PLUS] = isKeyPressed(KEY_NSPIRE_PLUS);
    d[K_MINUS] = isKeyPressed(KEY_NSPIRE_MINUS);
    d[K_MUL] = isKeyPressed(KEY_NSPIRE_MULTIPLY);
    d[K_DIV] = isKeyPressed(KEY_NSPIRE_DIVIDE);
    d[K_COMMA] = isKeyPressed(KEY_NSPIRE_COMMA);
    d[K_PERIOD] = isKeyPressed(KEY_NSPIRE_PERIOD);
    d[K_HOME] = isKeyPressed(KEY_NSPIRE_HOME);
    d[K_EQU] = isKeyPressed(KEY_NSPIRE_EQU);
    d[K_LP] = isKeyPressed(KEY_NSPIRE_LP);
    d[K_RP] = isKeyPressed(KEY_NSPIRE_RP);
    d[K_VAR] = isKeyPressed(KEY_NSPIRE_VAR);
    d[K_CAT] = isKeyPressed(KEY_NSPIRE_CAT);
    d[K_SCRATCH] = isKeyPressed(KEY_NSPIRE_SCRATCHPAD);
    d[K_EE] = isKeyPressed(KEY_NSPIRE_EE);
    d[K_NEG] = isKeyPressed(KEY_NSPIRE_NEGATIVE);
    d[K_TRIG] = isKeyPressed(KEY_NSPIRE_TRIG);
    d[K_POW] = isKeyPressed(KEY_NSPIRE_EXP);
    d[K_SQU] = isKeyPressed(KEY_NSPIRE_SQU);
    static const t_key *letters[26];
    static int init;
    if (!init) {
        const t_key *l[26] = { &KEY_NSPIRE_A, &KEY_NSPIRE_B, &KEY_NSPIRE_C, &KEY_NSPIRE_D, &KEY_NSPIRE_E, &KEY_NSPIRE_F,
            &KEY_NSPIRE_G, &KEY_NSPIRE_H, &KEY_NSPIRE_I, &KEY_NSPIRE_J, &KEY_NSPIRE_K, &KEY_NSPIRE_L, &KEY_NSPIRE_M,
            &KEY_NSPIRE_N, &KEY_NSPIRE_O, &KEY_NSPIRE_P, &KEY_NSPIRE_Q, &KEY_NSPIRE_R, &KEY_NSPIRE_S, &KEY_NSPIRE_T,
            &KEY_NSPIRE_U, &KEY_NSPIRE_V, &KEY_NSPIRE_W, &KEY_NSPIRE_X, &KEY_NSPIRE_Y, &KEY_NSPIRE_Z };
        memcpy(letters, l, sizeof l);
        init = 1;
    }
    for (int i = 0; i < 26; i++) d[K_A + i] = isKeyPressed(*letters[i]);
    const t_key *digits[10] = { &KEY_NSPIRE_0, &KEY_NSPIRE_1, &KEY_NSPIRE_2, &KEY_NSPIRE_3, &KEY_NSPIRE_4,
        &KEY_NSPIRE_5, &KEY_NSPIRE_6, &KEY_NSPIRE_7, &KEY_NSPIRE_8, &KEY_NSPIRE_9 };
    for (int i = 0; i < 10; i++) d[K_0 + i] = isKeyPressed(*digits[i]);
}

int plat_touch(int *dx, int *dy)
{
    *dx = *dy = 0;
    if (!is_touchpad) return 0;
    touchpad_report_t r;
    if (touchpad_scan(&r)) return 0;
    if (r.contact && tp_had && !r.pressed) {
        *dx = ((int)r.x - (int)tp_last.x) / 6;
        *dy = -((int)r.y - (int)tp_last.y) / 6;
    }
    if (r.contact && (*dx || *dy || !tp_had)) tp_last = r;
    tp_had = r.contact;
    return r.contact;
}

int plat_on_pressed(void) { return on_key_pressed(); }
const char *plat_start_dir(void) { return start_dir; }
const char *plat_arg_file(void) { return arg_file[0] ? arg_file : NULL; }
int plat_is_host(void) { return 0; }
void plat_log(const char *s) { (void)s; }

/* run on a private 2 MiB stack: QuickJS and the GLSL compiler recurse deeply */
static void (*run_fn)(void);
static void trampoline(void) { run_fn(); }
__attribute__((naked, noinline)) static void call_on_stack(void (*fn)(void), void *top)
{
    __asm__ volatile(
        "push {r4, lr}\n"
        "mov r4, sp\n"
        "mov sp, r1\n"
        "blx r0\n"
        "mov sp, r4\n"
        "pop {r4, pc}\n");
}
void plat_run(void (*fn)(void))
{
    const size_t SZ = 2 * 1024 * 1024;
    char *stk = malloc(SZ);
    if (!stk) { fn(); return; }
    run_fn = fn;
    void *top = (void *)(((uintptr_t)(stk + SZ)) & ~(uintptr_t)7);
    call_on_stack(trampoline, top);
    free(stk);
}
#endif
