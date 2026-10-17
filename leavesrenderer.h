#ifndef LEAVESRENDERER_H
#define LEAVESRENDERER_H

#include "blockrenderer.h"

#include "settingstask.h"

// Leaves of one kind of tree. With fancy leaves on (the Leaves setting) they
// use the see-through texture at (fancy_x, fancy_y), otherwise the block's
// own opaque one.
class LeavesRenderer : public NormalBlockRenderer
{
public:
    LeavesRenderer(BLOCK block, int fancy_x, int fancy_y) : block(block), fancy_x(fancy_x), fancy_y(fancy_y) {}

    virtual void geometryNormalBlock(const BLOCK_WDATA, const int local_x, const int local_y, const int local_z, const BLOCK_SIDE side, Chunk &c) override;
    virtual bool isOpaque(const BLOCK_WDATA /*block*/) override;
    virtual void drawPreview(const BLOCK_WDATA, TEXTURE &dest, int x, int y) override;

private:
    bool isTransparent();

    const BLOCK block;
    const int fancy_x, fancy_y;
};

#endif // LEAVESRENDERER_H
