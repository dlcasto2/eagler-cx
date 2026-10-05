#ifndef GRID_WORLD_H
#define GRID_WORLD_H

#include <set>
#include <tuple>

#include "physics.h"
#include "terrain.h"

// Test double: solid BLOCK_SIZE cubes at integer cells. Boxes that merely
// touch a cube don't count (strict overlap), unlike the game's AABB::intersects.
struct GridWorld : CollisionQuery {
    std::set<std::tuple<int, int, int>> solid;

    bool intersects(const AABB &b) const override
    {
        for(const auto &c : solid)
        {
            const GLFix lx = std::get<0>(c) * BLOCK_SIZE, ly = std::get<1>(c) * BLOCK_SIZE, lz = std::get<2>(c) * BLOCK_SIZE;
            if(b.high_x > lx && b.low_x < lx + BLOCK_SIZE
               && b.high_y > ly && b.low_y < ly + BLOCK_SIZE
               && b.high_z > lz && b.low_z < lz + BLOCK_SIZE)
                return true;
        }
        return false;
    }
};

// A 102-wide, 230-tall player box whose bottom centre sits at (x, y, z).
inline AABB playerBoxAt(int x, int y, int z)
{
    return AABB(GLFix(x - 51), GLFix(y), GLFix(z - 51), GLFix(x + 51), GLFix(y + 230), GLFix(z + 51));
}

#endif // GRID_WORLD_H
