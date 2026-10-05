/* Headless desktop test harness platform.
 * Env vars:
 *   NSPGL_FRAMES=n           quit after n frames
 *   NSPGL_SHOTS=f1,f2,...    write shot_<f>.png-able PPM at these frames (to NSPGL_OUT dir)
 *   NSPGL_KEYS=f:key[+],...  press key at frame f (released next frame), keys by name
 *   NSPGL_FPS=n              simulated frame time (default 30)            */
#ifndef _TINSPIRE
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint16_t fb[SCR_W * SCR_H];
static int frame, max_frames = 300, fps = 30;
static char outdir[256] = ".";
static char start_dir[256] = "./";
static char arg_file[256];
static int shots[64], nshots;
static struct { int frame, key, hold; } keys[256];
static int nkeys;

static const struct { const char *n; int k; } knames[] = {
    {"up",K_UP},{"down",K_DOWN},{"left",K_LEFT},{"right",K_RIGHT},{"enter",K_ENTER},{"esc",K_ESC},{"tab",K_TAB},
    {"menu",K_MENU},{"del",K_DEL},{"space",K_SPACE},{"shift",K_SHIFT},{"ctrl",K_CTRL},{"doc",K_DOC},{"click",K_CLICK},
    {"plus",K_PLUS},{"minus",K_MINUS},{"home",K_HOME},{NULL,0}};

int plat_init(int argc, char **argv)
{
    if (argc > 1) {
        snprintf(arg_file, sizeof arg_file, "%s", argv[1]);
        snprintf(start_dir, sizeof start_dir, "%s", argv[1]);
        char *s = strrchr(start_dir, '/'); if (s) s[1] = 0; else strcpy(start_dir, "./");
    }
    const char *e;
    if ((e = getenv("NSPGL_FRAMES"))) max_frames = atoi(e);
    if ((e = getenv("NSPGL_FPS"))) fps = atoi(e);
    if ((e = getenv("NSPGL_OUT"))) snprintf(outdir, sizeof outdir, "%s", e);
    if ((e = getenv("NSPGL_SHOTS"))) {
        char buf[1024]; snprintf(buf, sizeof buf, "%s", e);
        for (char *t = strtok(buf, ","); t && nshots < 64; t = strtok(NULL, ",")) shots[nshots++] = atoi(t);
    }
    if ((e = getenv("NSPGL_KEYS"))) {
        char buf[4096]; snprintf(buf, sizeof buf, "%s", e);
        for (char *t = strtok(buf, ","); t && nkeys < 256; t = strtok(NULL, ",")) {
            char *c = strchr(t, ':'); if (!c) continue;
            *c = 0;
            int f = atoi(t), hold = 1;
            char *nm = c + 1;
            char *star = strchr(nm, '*'); if (star) { *star = 0; hold = atoi(star + 1); }
            int k = 0;
            for (int i = 0; knames[i].n; i++) if (!strcmp(knames[i].n, nm)) k = knames[i].k;
            if (!k && strlen(nm) == 1 && nm[0] >= 'a' && nm[0] <= 'z') k = K_A + nm[0] - 'a';
            if (!k && strlen(nm) == 1 && nm[0] >= '0' && nm[0] <= '9') k = K_0 + nm[0] - '0';
            keys[nkeys].frame = f; keys[nkeys].key = k; keys[nkeys].hold = hold; nkeys++;
        }
    }
    return 0;
}
void plat_quit(void) {}
uint16_t *plat_fb(void) { return fb; }

static void write_ppm(int f)
{
    char fn[512]; snprintf(fn, sizeof fn, "%s/shot_%04d.ppm", outdir, f);
    FILE *o = fopen(fn, "wb");
    if (!o) return;
    fprintf(o, "P6 %d %d 255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        uint16_t c = fb[i];
        fputc(((c >> 11) & 31) * 255 / 31, o); fputc(((c >> 5) & 63) * 255 / 63, o); fputc((c & 31) * 255 / 31, o);
    }
    fclose(o);
    fprintf(stderr, "[host] wrote %s\n", fn);
}

static int shot_done[64];
void plat_present(void)
{
    for (int i = 0; i < nshots; i++) if (!shot_done[i] && shots[i] <= frame) { write_ppm(shots[i]); shot_done[i] = 1; }
}
uint32_t plat_ms(void) { return (uint32_t)(frame * 1000 / fps); }
void plat_wait_ms(int ms) { (void)ms; }
void plat_keys(uint8_t *d)
{
    frame++;
    memset(d, 0, K_COUNT);
    for (int i = 0; i < nkeys; i++) if (frame >= keys[i].frame && frame < keys[i].frame + keys[i].hold) d[keys[i].key] = 1;
}
int plat_touch(int *dx, int *dy) { *dx = *dy = 0; return 0; }
int plat_on_pressed(void) { return frame >= max_frames; }
const char *plat_start_dir(void) { return start_dir; }
const char *plat_arg_file(void) { return arg_file[0] ? arg_file : NULL; }
int plat_is_host(void) { return 1; }
void plat_log(const char *s) { fprintf(stderr, "%s\n", s); }
void plat_run(void (*fn)(void)) { fn(); }
#endif
