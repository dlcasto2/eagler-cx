#ifndef SAPLINGRENDERER_H
#define SAPLINGRENDERER_H

#include "billboardrenderer.h"

// A sapling (oak, spruce or birch by its data): a billboard that, on dirt or
// grass, now and then grows into its tree using the world generator's shape.
class SaplingRenderer : public BillboardRenderer
{
public:
    SaplingRenderer();

    virtual void tick(const BLOCK_WDATA block, int local_x, int local_y, int local_z, Chunk &c) override;

    // Chunks tick every 10th frame; about 1 in 600 of those grows it, so a
    // sapling takes a few minutes on the calculator, like a 1.8.8 sapling.
    static constexpr int GROW_CHANCE = 600;

    static bool canStayOn(BLOCK below);
};

#endif // SAPLINGRENDERER_H
