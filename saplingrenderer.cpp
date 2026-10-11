#include "saplingrenderer.h"

#include <cstdlib>

#include "chunk.h"

SaplingRenderer::SaplingRenderer()
{
    setEntry(0, 15, 0, "Oak Sapling", BLOCK_SIZE * 3 / 4, BLOCK_SIZE * 3 / 4, BLOCK_SIZE * 3 / 4);
}

bool SaplingRenderer::canStayOn(BLOCK below)
{
    return below == BLOCK_DIRT || below == BLOCK_GRASS;
}

void SaplingRenderer::tick(const BLOCK_WDATA /*block*/, int local_x, int local_y, int local_z, Chunk &c)
{
    if(!canStayOn(getBLOCK(c.getGlobalBlockRelative(local_x, local_y - 1, local_z))))
        return;
    if(rand() % GROW_CHANCE != 0)
        return;
    c.growTree(local_x, local_y, local_z);
}
