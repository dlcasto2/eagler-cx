#include "doctest.h"
#include "trees.h"

#include <cstdlib>
#include <set>
#include <tuple>

namespace {

struct Shape {
    int n = 0, logs = 0, leaves = 0, height = 0, top_leaf = -1, radius = 0;
    std::set<std::tuple<int, int, int>> cells;
};

Shape describe(TreeKind kind, uint32_t rnd)
{
    static TreeBlock out[MAX_TREE_BLOCKS];
    Shape s;
    s.n = treeShape(kind, rnd, 30, out);
    for(int i = 0; i < s.n; ++i)
    {
        const TreeBlock &t = out[i];
        CHECK(s.cells.insert(std::make_tuple(t.dx, t.dy, t.dz)).second);   // nothing placed twice
        if(t.block == logFor(kind))
        {
            ++s.logs;
            CHECK(t.dx == 0); CHECK(t.dz == 0);
            s.height = std::max(s.height, t.dy + 1);
        }
        else
        {
            CHECK(t.block == leavesFor(kind));
            ++s.leaves;
            s.top_leaf = std::max(s.top_leaf, static_cast<int>(t.dy));
            s.radius = std::max(s.radius, std::max(std::abs(t.dx), std::abs(t.dz)));
        }
    }
    return s;
}

} // namespace

TEST_CASE("oak and birch: a straight trunk under a round crown") {
    for(uint32_t r = 0; r < 200; ++r)
    {
        const Shape oak = describe(TreeKind::OAK, r * 7919u);
        CHECK(oak.height >= 4); CHECK(oak.height <= 6);
        CHECK(oak.logs == oak.height);                 // trunk from the ground up, no gaps
        CHECK(oak.top_leaf == oak.height);             // leaves cap the trunk
        CHECK(oak.radius == 2);
        CHECK(oak.leaves >= 20);

        const Shape birch = describe(TreeKind::BIRCH, r * 104729u);
        CHECK(birch.height >= 5); CHECK(birch.height <= 7);
        CHECK(birch.logs == birch.height);
    }
}

TEST_CASE("spruce: tall with rings of leaves") {
    int widest = 0;
    for(uint32_t r = 0; r < 200; ++r)
    {
        const Shape s = describe(TreeKind::SPRUCE, r * 15485863u);
        CHECK(s.logs >= 4);
        CHECK(s.top_leaf >= 6); CHECK(s.top_leaf <= 9);
        CHECK(s.radius >= 1); CHECK(s.radius <= 3);
        CHECK(s.cells.count(std::make_tuple(0, s.top_leaf, 0)) == 1);   // a point on top
        widest = std::max(widest, s.radius);
    }
    CHECK(widest == 3);
}

TEST_CASE("trees that wouldn't fit under the sky don't grow") {
    TreeBlock out[MAX_TREE_BLOCKS];
    CHECK(treeShape(TreeKind::OAK, 1, 4, out) == 0);
    CHECK(treeShape(TreeKind::SPRUCE, 1, 6, out) == 0);
    CHECK(treeShape(TreeKind::OAK, 1, 7, out) > 0);
}

TEST_CASE("every kind has its own wood and leaves") {
    CHECK(logFor(TreeKind::OAK) == BLOCK_WOOD); CHECK(leavesFor(TreeKind::OAK) == BLOCK_LEAVES);
    CHECK(logFor(TreeKind::SPRUCE) == BLOCK_SPRUCE_LOG); CHECK(leavesFor(TreeKind::SPRUCE) == BLOCK_SPRUCE_LEAVES);
    CHECK(logFor(TreeKind::BIRCH) == BLOCK_BIRCH_LOG); CHECK(leavesFor(TreeKind::BIRCH) == BLOCK_BIRCH_LEAVES);
}
