#include "../src/gl/glsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *vs =
"attribute vec3 aPos; attribute vec3 aCol;\n"
"uniform mat4 uMVP;\n varying vec3 vCol;\n"
"#define SCALE 2.0\n"
"vec3 tw(vec3 c, out float k){ k = 3.0; return c*SCALE; }\n"
"void main(){ float k; vCol = tw(aCol, k) * 0.5 + vec3(k-3.0); gl_Position = uMVP * vec4(aPos,1.0); }\n";
static const char *fs =
"precision mediump float;\n varying vec3 vCol; uniform float t;\n"
"struct L { vec3 p; float i; }; uniform L lights[2];\n"
"float f(int n){ float s=0.0; for(int i=0;i<10;i++){ if(i>=n) break; s += float(i);} return s; }\n"
"void main(){ vec4 c = vec4(vCol,1.0); c.rg = c.gr; float s = f(4);\n"
" if (s > 5.0) c.b = smoothstep(0.0, 1.0, 0.5); vec3 acc = vec3(0.0);\n"
" for (int i=0;i<2;i++) acc += lights[i].p * lights[i].i;\n"
" gl_FragColor = c + vec4(acc, 0.0) + vec4(s*0.0); }\n";
int main(){
  GShader *v = glsl_compile(vs, 0), *f = glsl_compile(fs, 1);
  printf("vs ok=%d log=%s ncode=%d mem=%d\n", v->ok, v->log, v->ncode, v->memsize);
  printf("fs ok=%d log=%s ncode=%d mem=%d\n", f->ok, f->log, f->ncode, f->memsize);
  for (int i=0;i<v->nvars;i++) printf(" v %s %s off=%d st=%d\n", v->vars[i].name, glsl_type_name(v->vars[i].type), v->vars[i].off, v->vars[i].storage);
  for (int i=0;i<f->nvars;i++) printf(" f %s %s off=%d st=%d\n", f->vars[i].name, glsl_type_name(f->vars[i].type), f->vars[i].off, f->vars[i].storage);
  float *m = malloc(v->memsize*4); memcpy(m, v->init, v->memsize*4);
  int mvp=-1, pos=-1, col=-1, vc=-1;
  for (int i=0;i<v->nvars;i++){ if(!strcmp(v->vars[i].name,"uMVP"))mvp=v->vars[i].off; if(!strcmp(v->vars[i].name,"aPos"))pos=v->vars[i].off; if(!strcmp(v->vars[i].name,"aCol"))col=v->vars[i].off; if(!strcmp(v->vars[i].name,"vCol"))vc=v->vars[i].off;}
  for(int i=0;i<16;i++) m[mvp+i] = (i%5==0)?2:0; m[mvp+12]=1;
  m[pos]=1;m[pos+1]=2;m[pos+2]=3; m[col]=0.1;m[col+1]=0.2;m[col+2]=0.3;
  int r = glsl_run(v, m, 0, 0);
  printf("run=%d pos=%g %g %g %g vcol=%g %g %g\n", r, m[v->off_position],m[v->off_position+1],m[v->off_position+2],m[v->off_position+3], m[vc],m[vc+1],m[vc+2]);
  float *fm = malloc(f->memsize*4); memcpy(fm, f->init, f->memsize*4);
  for (int i=0;i<f->nvars;i++){ GVar *g=&f->vars[i]; if(!strcmp(g->name,"vCol")){fm[g->off]=0.1;fm[g->off+1]=0.2;fm[g->off+2]=0.3;}
    if(!strcmp(g->name,"lights[1].p")){fm[g->off]=1;fm[g->off+1]=2;fm[g->off+2]=3;} if(!strcmp(g->name,"lights[1].i")) fm[g->off]=0.5; }
  r = glsl_run(f, fm, 0, 0);
  printf("run=%d color=%g %g %g %g\n", r, fm[f->off_fragcolor],fm[f->off_fragcolor+1],fm[f->off_fragcolor+2],fm[f->off_fragcolor+3]);
  GShader *bad = glsl_compile("void main(){ gl_FragColor = vec3(1.0); }", 1); printf("bad ok=%d log=%s", bad->ok, bad->log);
}
