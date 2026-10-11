#include "containers.h"

void chestPlaced(int x, int y, int z)
{
    block_storage.create(BlockPos{x, y, z}, StorageKind::CHEST);
}

std::vector<ItemStack> chestBroken(int x, int y, int z)
{
    std::vector<ItemStack> out;
    const BlockPos p{x, y, z};
    StorageRecord *r = block_storage.find(p);
    if(!r)
        return out;
    for(const ItemStack &s : r->slots)
        if(!s.empty())
            out.push_back(s);
    block_storage.erase(p);
    return out;
}

ItemStack smithingResult(const ItemStack &tool, const ItemStack &ingot)
{
    if(tool.empty() || ingot.empty() || ingot.id != ITEM_NETHERITE_INGOT)
        return ItemStack();
    for(ToolKind k : { ToolKind::PICKAXE, ToolKind::AXE, ToolKind::SHOVEL, ToolKind::SWORD })
        if(tool.id == toolId(ToolMaterial::DIAMOND, k))
            return ItemStack(toolId(ToolMaterial::NETHERITE, k), 1, tool.meta);
    return ItemStack();
}

BLOCK_SIDE bedFacingFromYaw(int yaw_degrees)
{
    const int y = ((yaw_degrees % 360) + 360) % 360;
    if(y < 45 || y >= 315) return BLOCK_BACK;
    if(y < 135) return BLOCK_RIGHT;
    if(y < 225) return BLOCK_FRONT;
    return BLOCK_LEFT;
}

void sideOffset(BLOCK_SIDE side, int &dx, int &dz)
{
    dx = dz = 0;
    switch(side)
    {
    case BLOCK_FRONT: dz = -1; break;
    case BLOCK_BACK:  dz = 1;  break;
    case BLOCK_LEFT:  dx = -1; break;
    case BLOCK_RIGHT: dx = 1;  break;
    default: break;
    }
}

void bedOtherHalf(int x, int z, uint8_t data, int &other_x, int &other_z)
{
    int dx, dz;
    sideOffset(bedFacing(data), dx, dz);
    const int dir = (data & BED_HEAD) ? -1 : 1;   // the head points back to the foot
    other_x = x + dx * dir;
    other_z = z + dz * dir;
}

SleepCheck canSleep(uint32_t time_of_day, Difficulty difficulty)
{
    if(difficulty == Difficulty::TORMENT)
        return SleepCheck::TORMENT;
    const uint32_t t = time_of_day % 24000;
    if(t < 12541 || t > 23458)
        return SleepCheck::NOT_NIGHT;
    return SleepCheck::OK;
}

void sleepInBed(int x, int y, int z)
{
    world_state.time_of_day = 0;
    player_state.spawn_x = x;
    player_state.spawn_y = y;
    player_state.spawn_z = z;
    player_state.spawn_is_bed = true;
}
