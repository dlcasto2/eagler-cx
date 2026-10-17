#include "trees.h"

namespace {

// Java's LCG would do; this is a small one of our own.
struct Rng {
    uint32_t s;
    int next(int n) { s = s * 1103515245u + 12345u; return static_cast<int>((s >> 16) % static_cast<uint32_t>(n)); }
};

struct Builder {
    TreeBlock *out;
    int n = 0;
    void add(int dx, int dy, int dz, BLOCK b)
    {
        if(n < MAX_TREE_BLOCKS)
            out[n++] = TreeBlock{ static_cast<int8_t>(dx), static_cast<int8_t>(dy), static_cast<int8_t>(dz), b };
    }
};

int iabs(int v) { return v < 0 ? -v : v; }

// Oak and birch: a straight trunk under two wide and two narrow layers of
// leaves. Corners of the lower three layers are left out at random, the top
// layer's always.
int roundTree(TreeKind kind, int height, Rng &r, Builder &b)
{
    for(int y = 0; y < height; ++y)
        b.add(0, y, 0, logFor(kind));

    for(int y = height - 3; y <= height; ++y)
    {
        const int k = y - height;           // -3 .. 0
        const int radius = 1 - k / 2;       // 2, 2, 1, 1 (truncating, as Java does)
        for(int dx = -radius; dx <= radius; ++dx)
            for(int dz = -radius; dz <= radius; ++dz)
            {
                if(dx == 0 && dz == 0 && y < height)
                    continue;               // the trunk
                const bool corner = iabs(dx) == radius && iabs(dz) == radius;
                if(!corner || (k != 0 && r.next(2) != 0))
                    b.add(dx, y, dz, leavesFor(kind));
            }
    }
    return b.n;
}

// Spruce: rings of leaves that widen and narrow down the trunk, with a bare
// stretch at the bottom and a point at the top.
int spruce(int height, Rng &r, Builder &b)
{
    const int bare = 1 + r.next(2);
    const int crown = height - bare;
    const int max_radius = 2 + r.next(2);
    const int trunk_top = height - r.next(3);

    for(int y = 0; y < trunk_top; ++y)
        b.add(0, y, 0, BLOCK_SPRUCE_LOG);

    int radius = r.next(2), step_max = 1, restart = 0;
    for(int i = 0; i <= crown; ++i)
    {
        const int y = height - i;
        for(int dx = -radius; dx <= radius; ++dx)
            for(int dz = -radius; dz <= radius; ++dz)
            {
                if(dx == 0 && dz == 0 && y < trunk_top)
                    continue;               // the trunk
                if(radius == 0 || iabs(dx) != radius || iabs(dz) != radius)
                    b.add(dx, y, dz, BLOCK_SPRUCE_LEAVES);
            }

        if(radius >= step_max)
        {
            radius = restart;
            restart = 1;
            if(++step_max > max_radius)
                step_max = max_radius;
        }
        else
            ++radius;
    }
    return b.n;
}

} // namespace

BLOCK logFor(TreeKind k)
{
    return k == TreeKind::SPRUCE ? BLOCK_SPRUCE_LOG : k == TreeKind::BIRCH ? BLOCK_BIRCH_LOG : BLOCK_WOOD;
}

BLOCK leavesFor(TreeKind k)
{
    return k == TreeKind::SPRUCE ? BLOCK_SPRUCE_LEAVES : k == TreeKind::BIRCH ? BLOCK_BIRCH_LEAVES : BLOCK_LEAVES;
}

int treeShape(TreeKind kind, uint32_t rnd, int room, TreeBlock *out)
{
    Rng r{ rnd };
    Builder b;
    b.out = out;

    int height;
    switch(kind)
    {
    case TreeKind::SPRUCE: height = 6 + r.next(4); break;    // 6-9
    case TreeKind::BIRCH:  height = 5 + r.next(3); break;    // 5-7
    default:               height = 4 + r.next(3); break;    // 4-6
    }

    // The top leaves sit at y = height.
    if(height + 1 > room)
        return 0;

    return kind == TreeKind::SPRUCE ? spruce(height, r, b) : roundTree(kind, height, r, b);
}
