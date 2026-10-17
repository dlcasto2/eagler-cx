#include "water.h"

void fillSeaColumn(BLOCK_WDATA blocks[8][8][8], int x, int z, int chunk_y, int ground_height)
{
    const int base = chunk_y * 8;
    const int from = ground_height > base ? ground_height - base : 0;
    const int to = SEA_LEVEL - base < 8 ? SEA_LEVEL - base : 8;
    for(int y = from; y < to; ++y)
        if(getBLOCK(blocks[x][y][z]) == BLOCK_AIR)
            blocks[x][y][z] = WATER_SOURCE;
}

bool becomesWaterSource(BLOCK_WDATA self, const BLOCK_WDATA sides[4], BLOCK_WDATA below, bool below_is_solid)
{
    if(getBLOCK(self) != BLOCK_WATER || isWaterSource(self))
        return false;

    int sources = 0;
    for(int i = 0; i < 4; ++i)
        if(isWaterSource(sides[i]))
            ++sources;

    return sources >= 2 && (below_is_solid || isWaterSource(below));
}

BLOCK lavaHardensInto(BLOCK_WDATA lava, bool touches_water)
{
    if(getBLOCK(lava) != BLOCK_LAVA || !touches_water)
        return BLOCK_AIR;
    return isLavaSource(lava) ? BLOCK_OBSIDIAN : BLOCK_COBBLESTONE;
}

uint16_t nextAirTicks(uint16_t air, bool head_underwater, bool survival)
{
    if(!head_underwater || !survival)
        return MAX_AIR_TICKS;
    return air > 0 ? air - 1 : 0;
}
