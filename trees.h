#ifndef TREES_H
#define TREES_H

#include <cstdint>

#include "terrain.h"

// The three trees of the overworld biomes, shaped like 1.8.8's generators:
// oak (WorldGenTrees), birch (WorldGenForest) and spruce (WorldGenTaiga2).
// A sapling's data is its tree kind.
enum class TreeKind : uint8_t { OAK = 0, SPRUCE = 1, BIRCH = 2 };
constexpr int TREE_KINDS = 3;

BLOCK logFor(TreeKind k);
BLOCK leavesFor(TreeKind k);

// Flower data for the dead bush, which shares the flower's billboard renderer.
constexpr uint8_t FLOWER_DEAD_BUSH = 2;

struct TreeBlock {
    int8_t dx, dy, dz;   // from the bottom of the trunk
    BLOCK block;
};

constexpr int MAX_TREE_BLOCKS = 160;

// Lays out one tree whose trunk starts at (0, 0, 0). rnd picks its height and
// leaf corners. room is how many blocks there are above the ground; returns
// the block count, or 0 if the tree wouldn't fit. Logs come first.
int treeShape(TreeKind kind, uint32_t rnd, int room, TreeBlock *out);

#endif // TREES_H
