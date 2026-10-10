#include "oregen.h"

namespace {

// A well-mixed hash of a world position, the seed and a salt per ore.
uint32_t hash(int x, int y, int z, uint32_t seed, uint32_t salt)
{
    uint32_t h = seed ^ salt;
    h ^= static_cast<uint32_t>(x) * 0x8DA6B343u;
    h ^= static_cast<uint32_t>(y) * 0xD8163841u;
    h ^= static_cast<uint32_t>(z) * 0xCB1AB31Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du;
    h ^= h >> 12; h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

struct OreRule {
    BLOCK ore;
    int min_y, max_y;
    uint32_t seed_per_100k;   // chance a stone block starts a cluster
    int max_extra;            // extra blocks in the cluster, 0..max_extra
    uint32_t salt;
};

const OreRule rules[] = {
    { BLOCK_GOLD_ORE,       1, DEEP_ORE_MAX_Y, 220, 2, 0x601D },
    { BLOCK_EMERALD_ORE,    1, DEEP_ORE_MAX_Y, 220, 2, 0xE3E2 },
    { BLOCK_ANCIENT_DEBRIS, 1, DEBRIS_MAX_Y,    90, 1, 0xDEB2 },
};

// Neighbours a cluster grows into, in order.
const int grow[][3] = { {1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-1, 0, 0} };

} // namespace

void generateOres(BLOCK_WDATA blocks[8][8][8], int chunk_x, int chunk_y, int chunk_z, uint32_t world_seed)
{
    for(const OreRule &r : rules)
    {
        for(int x = 0; x < 8; ++x)
            for(int y = 0; y < 8; ++y)
                for(int z = 0; z < 8; ++z)
                {
                    const int gx = chunk_x * 8 + x, gy = chunk_y * 8 + y, gz = chunk_z * 8 + z;
                    if(gy < r.min_y || gy > r.max_y || getBLOCK(blocks[x][y][z]) != BLOCK_STONE)
                        continue;

                    const uint32_t h = hash(gx, gy, gz, world_seed, r.salt);
                    if(h % 100000 >= r.seed_per_100k)
                        continue;

                    blocks[x][y][z] = r.ore;
                    const int extra = static_cast<int>((h >> 20) % static_cast<uint32_t>(r.max_extra + 1));
                    for(int i = 0; i < extra; ++i)
                    {
                        const int nx = x + grow[i][0], ny = y + grow[i][1], nz = z + grow[i][2];
                        const int ngy = gy + grow[i][1];
                        if(nx < 0 || nx > 7 || ny < 0 || ny > 7 || nz < 0 || nz > 7 || ngy > r.max_y)
                            continue;
                        if(getBLOCK(blocks[nx][ny][nz]) == BLOCK_STONE)
                            blocks[nx][ny][nz] = r.ore;
                    }
                }
    }
}
