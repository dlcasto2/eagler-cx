#include "../src/gl/glsl_int.h"
#include <stdio.h>
#include <stdlib.h>
static const char *names[]={"NOP","MOV","SWZ","SCAT","ADD","SUB","MUL","DIV","NEG","MOD","MIN","MAX","POW","ATAN2","STEP","CLAMP","MIX","SSTEP","UFN","DOT","CROSS","LEN","DIST","NORM","REFLECT","REFRACT","FFWD","MATVEC","VECMAT","MATMAT","LT","LE","GT","GE","EQC","NEC","EQ","NE","ANY","ALL","AND","OR","XOR","JMP","JZ","JNZ","KILL","TEX","TEXPROJ","TEXCUBE","IDXLD","IDXST","MATDIAG","MATRESIZE","END"};
int main(int c, char **v){ FILE*f=fopen(v[1],"rb"); static char b[65536]; b[fread(b,1,65535,f)]=0;
 GShader *s=glsl_compile(b, atoi(v[2])); printf("ok=%d %s\n", s->ok, s->log);
 for(int i=0;i<s->ncode;i++){GOp*o=&s->code[i]; printf("%3d %-8s n=%d fl=%d d=%d a=%d b=%d c=%d\n",i,names[o->op],o->n,o->fl,o->d,o->a,o->b,o->c);} }
