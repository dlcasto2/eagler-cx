#include "saplingrenderer.h"

#include <cstdlib>

#include "chunk.h"
#include "trees.h"

SaplingRenderer::SaplingRenderer()
{
    const GLFix size = BLOCK_SIZE * 3 / 4;
    setEntry(static_cast<uint8_t>(TreeKind::OAK), 15, 0, "Oak Sapling", size, size, size);
    setEntry(static_cast<uint8_t>(TreeKind::SPRUCE), 15, 3, "Spruce Sapling", size, size, size);
    setEntry(static_cast<uint8_t>(TreeKind::BIRCH), 15, 4, "Birch Sapling", size, size, size);
}

bool SaplingRenderer::canStayOn(BLOCK below)
{
    return below == BLOCK_DIRT || below == BLOCK_GRASS || below == BLOCK_SNOWY_GRASS;
}

void SaplingRenderer::tick(const BLOCK_WDATA /*block*/, int local_x, int local_y, int local_z, Chunk &c)
{
    if(!canStayOn(getBLOCK(c.getGlobalBlockRelative(local_x, local_y - 1, local_z))))
        return;
    if(rand() % GROW_CHANCE != 0)
        return;
    c.growTree(local_x, local_y, local_z);
}
