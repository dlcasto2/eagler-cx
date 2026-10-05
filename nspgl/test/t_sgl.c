#include "../src/gl/sgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void ppm(SGL *g, const char *fn){ FILE*f=fopen(fn,"wb"); fprintf(f,"P6 %d %d 255\n",g->w,g->h);
 for(int y=g->h-1;y>=0;y--) for(int x=0;x<g->w;x++){ uint32_t c=g->color[y*g->w+x]; fputc(c&255,f);fputc((c>>8)&255,f);fputc((c>>16)&255,f);} fclose(f);}
static int mk(SGL*g,const char*vs,const char*fs){ int v=sgl_create_shader(g,GL_VERTEX_SHADER),f=sgl_create_shader(g,GL_FRAGMENT_SHADER);
 sgl_shader_source(g,v,vs); sgl_shader_source(g,f,fs); int p=sgl_create_program(g); sgl_attach_shader(g,p,v); sgl_attach_shader(g,p,f); sgl_link_program(g,p);
 SGLProgram*P=sgl_obj(&g->programs,p); if(!P->link_ok) printf("link fail: %s\n",P->log); return p;}
int main(){
 SGL *g = sgl_create(160,120,160,120,1,0);
 g->clear_color[0]=0.1;g->clear_color[1]=0.1;g->clear_color[2]=0.2;g->clear_color[3]=1;
 sgl_clear(g, GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 int p = mk(g,"attribute vec2 p; attribute vec3 c; varying vec3 vc; void main(){ vc=c; gl_Position=vec4(p,0.0,1.0);}",
  "precision mediump float; varying vec3 vc; void main(){ gl_FragColor=vec4(vc,1.0);}");
 float data[]={ -0.8,-0.8, 1,0,0,  0.8,-0.8, 0,1,0,  0,0.8, 0,0,1 };
 int b=sgl_create_buffer(g); sgl_bind_buffer(g,GL_ARRAY_BUFFER,b); sgl_buffer_data(g,GL_ARRAY_BUFFER,data,sizeof data,0);
 sgl_use_program(g,p); int lp=sgl_get_attrib_location(g,p,"p"), lc=sgl_get_attrib_location(g,p,"c");
 sgl_vertex_attrib_pointer(g,lp,2,GL_FLOAT,0,20,0); sgl_enable_attrib(g,lp,1);
 sgl_vertex_attrib_pointer(g,lc,3,GL_FLOAT,0,20,8); sgl_enable_attrib(g,lc,1);
 sgl_draw_arrays(g,GL_TRIANGLES,0,3);
 printf("err=%x tris=%lld px=%lld\n", g->error, g->stat_tris, g->stat_pixels);
 ppm(g,"/tmp/tri.ppm");
 /* fullscreen shader test with gl_FragCoord */
 int p2 = mk(g,"attribute vec2 p; void main(){ gl_Position=vec4(p,0.0,1.0);}",
  "precision mediump float; uniform vec2 res; uniform float t; void main(){ vec2 uv=gl_FragCoord.xy/res; float d=length(uv-0.5); gl_FragColor=vec4(0.5+0.5*sin(d*30.0-t), uv, 1.0);}");
 float q[]={-1,-1, 1,-1, -1,1, 1,1}; int b2=sgl_create_buffer(g); sgl_bind_buffer(g,GL_ARRAY_BUFFER,b2); sgl_buffer_data(g,GL_ARRAY_BUFFER,q,sizeof q,0);
 sgl_use_program(g,p2); int l=sgl_get_attrib_location(g,p2,"p"); sgl_enable_attrib(g,lc,0); sgl_vertex_attrib_pointer(g,l,2,GL_FLOAT,0,0,0); sgl_enable_attrib(g,l,1);
 int e; int ur=sgl_get_uniform_location(g,p2,"res",&e); float rv[2]={160,120}; sgl_uniform(g,p2,ur,0,rv,2);
 sgl_draw_arrays(g,GL_TRIANGLE_STRIP,0,4); printf("err=%x px=%lld\n", g->error, g->stat_pixels); ppm(g,"/tmp/fs.ppm");
}
