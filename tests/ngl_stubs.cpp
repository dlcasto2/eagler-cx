// No-op stand-ins for nGL drawing calls that host-compiled code references
// (AABB::render) but tests never execute.
#include "gl.h"

void nglAddVertex(const VERTEX *) {}
void glBegin(const GLDrawMode) {}

#include "terrain.h"
TEXTURE *terrain_current = nullptr;
TEXTURE *terrain_quad = nullptr;
