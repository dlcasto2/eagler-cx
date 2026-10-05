#include "stresstest.h"

#include "terrain.h"

namespace {

// Deterministic LCG so a run is reproducible from its seed.
uint32_t lcg(uint32_t &state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

} // namespace

int spawnStressDummies(EntityPool &pool, GLFix px, GLFix py, GLFix pz, uint32_t seed)
{
    uint32_t state = seed ? seed : 1;
    int spawned = 0;

    for(int i = 0; i < EntityPool::CAPACITY; ++i)
    {
        // Distance 3..12 blocks, any of 16 compass directions.
        const int dist = 3 + static_cast<int>(lcg(state) % 10);
        const int dir = static_cast<int>(lcg(state) % 16);

        // 16-point unit vectors scaled by 1000, to stay off floats.
        static const int sin16[16] = { 0, 383, 707, 924, 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383 };
        static const int cos16[16] = { 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924 };

        const GLFix dx = GLFix(dist) * BLOCK_SIZE * cos16[dir] / 1000;
        const GLFix dz = GLFix(dist) * BLOCK_SIZE * sin16[dir] / 1000;

        const GLFix cx = px + dx, cy = py + BLOCK_SIZE * 2, cz = pz + dz;
        const AABB box{cx - BLOCK_SIZE / 2, cy, cz - BLOCK_SIZE / 2, cx + BLOCK_SIZE / 2, cy + BLOCK_SIZE, cz + BLOCK_SIZE / 2};

        Entity *e = pool.spawn(EntityType::STRESS_DUMMY, box);
        if(!e)
            break;
        ++spawned;
    }

    return spawned;
}

int stressBrightnessForTick(uint32_t tick)
{
    return 7 - static_cast<int>((tick / 100) % 8);
}

void stressSteer(Entity &e, uint32_t tick, uint32_t seed)
{
    if(tick % 40 != 0)
        return;

    uint32_t state = seed ^ (tick * 2654435761u);
    const int dir = static_cast<int>(lcg(state) % 16);
    static const int sin16[16] = { 0, 383, 707, 924, 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383 };
    static const int cos16[16] = { 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924 };

    e.vx = GLFix(cos16[dir]) * 10 / 1000;
    e.vz = GLFix(sin16[dir]) * 10 / 1000;
}
