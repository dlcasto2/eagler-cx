#include "mining.h"

#include "blockdefs.h"
#include "trees.h"

namespace {

const ItemDef &toolDef(ItemId tool)
{
    return itemDef(tool);
}

// Speed x100, before the water and air penalties.
unsigned speedX100(BLOCK b, ItemId tool)
{
    if(tool == 0 || isBlockItem(tool))
        return 100;
    const ItemDef &t = toolDef(tool);
    if(t.tool == ToolKind::SWORD)
        return b == BLOCK_SPIDERWEB ? 1500 : 150;
    if(t.tool != ToolKind::NONE && t.tool == blockDef(b).best_tool)
        return 100u * t.mining_speed;
    return 100;
}

uint32_t nextRandom(uint32_t &rng)
{
    rng = rng * 1103515245u + 12345u;
    return rng >> 8;
}

} // namespace

bool canHarvest(BLOCK_WDATA target, ItemId tool)
{
    const BlockDef &d = blockDef(getBLOCK(target));
    if(d.min_tier == 0)
        return true;
    if(tool == 0 || isBlockItem(tool))
        return false;
    const ItemDef &t = toolDef(tool);
    return t.tool == d.best_tool && t.harvest_tier >= d.min_tier;
}

int miningTicks(BLOCK_WDATA target, ItemId tool, bool in_water, bool on_ground)
{
    const BLOCK b = getBLOCK(target);
    const BlockDef &d = blockDef(b);
    if(!d.defined || d.hardness_x100 == UNBREAKABLE)
        return MINING_NEVER;
    if(d.hardness_x100 == 0)
        return 0;

    unsigned penalty = 1;
    if(in_water) penalty *= 5;
    if(!on_ground) penalty *= 5;

    const unsigned divisor = canHarvest(target, tool) ? 30 : 100;
    const unsigned work = divisor * d.hardness_x100 * penalty;   // 1 / (damage per tick), x speed
    const unsigned speed = speedX100(b, tool);
    if(speed >= work)
        return 0;   // more than a whole block per tick: instant
    return static_cast<int>((work + speed - 1) / speed);
}

ItemStack blockDropFor(BLOCK_WDATA target, ItemId tool, uint32_t &rng)
{
    const BLOCK b = getBLOCK(target);
    const BlockDef &d = blockDef(b);
    if(!d.defined || !canHarvest(target, tool))
        return ItemStack();

    // Leaves (1.8.8): their own sapling 1 time in 20, otherwise from oak an
    // apple 1 time in 200.
    if(b == BLOCK_LEAVES || b == BLOCK_SPRUCE_LEAVES || b == BLOCK_BIRCH_LEAVES)
    {
        const TreeKind kind = b == BLOCK_SPRUCE_LEAVES ? TreeKind::SPRUCE : b == BLOCK_BIRCH_LEAVES ? TreeKind::BIRCH : TreeKind::OAK;
        const uint32_t roll = nextRandom(rng) % 1000;
        if(roll < 50)
            return ItemStack(BLOCK_SAPLING, 1, static_cast<uint16_t>(kind));
        if(roll < 55 && kind == TreeKind::OAK)
            return ItemStack(ITEM_APPLE, 1);
        return ItemStack();
    }

    // A dead bush (1.8.8): up to two sticks.
    if(b == BLOCK_FLOWER && getBLOCKDATA(target) == FLOWER_DEAD_BUSH)
    {
        const uint8_t sticks = static_cast<uint8_t>(nextRandom(rng) % 3);
        return sticks ? ItemStack(ITEM_STICK, sticks) : ItemStack();
    }

    if(d.drop.item == 0)
        return ItemStack();
    if(d.drop.chance_permille < 1000 && nextRandom(rng) % 1000 >= d.drop.chance_permille)
        return ItemStack();

    unsigned count = d.drop.min;
    if(d.drop.max > d.drop.min)
        count += nextRandom(rng) % (d.drop.max - d.drop.min + 1u);
    if(count == 0)
        return ItemStack();

    if(d.drop.item != DROP_SELF)
        return ItemStack(d.drop.item, static_cast<uint8_t>(count));

    // Flowers, mushrooms and saplings keep their kind; everything else drops
    // without its orientation or state.
    const bool keep_data = b == BLOCK_FLOWER || b == BLOCK_MUSHROOM || b == BLOCK_SAPLING;
    return ItemStack(b, static_cast<uint8_t>(count), keep_data ? getBLOCKDATA(target) : 0);
}

int toolWearFor(ItemId tool, BLOCK_WDATA target)
{
    if(tool == 0 || isBlockItem(tool))
        return 0;
    const ItemDef &t = toolDef(tool);
    if(t.durability == 0 || t.tool == ToolKind::BOW)
        return 0;
    if(blockDef(getBLOCK(target)).hardness_x100 == 0)
        return 0;
    return t.tool == ToolKind::SWORD ? 2 : 1;
}

void wearTool(ItemStack &tool, int amount)
{
    if(tool.empty() || amount <= 0)
        return;
    const ItemDef &t = toolDef(tool.id);
    if(t.durability == 0)
        return;
    const unsigned wear = tool.meta + static_cast<unsigned>(amount);
    if(wear >= t.durability)
        tool = ItemStack();
    else
        tool.meta = static_cast<uint16_t>(wear);
}

bool miningTick(MiningState &m, bool holding, bool has_target, int x, int y, int z,
                BLOCK_WDATA looked_at, ItemId tool, bool in_water, bool on_ground)
{
    if(m.cooldown > 0)
        --m.cooldown;

    if(!holding || !has_target)
    {
        m.active = false;
        m.progress = 0;
        return false;
    }

    if(!m.active || m.x != x || m.y != y || m.z != z || m.block != looked_at)
    {
        m.active = true;
        m.x = x; m.y = y; m.z = z;
        m.block = looked_at;
        m.progress = 0;
    }

    if(m.cooldown > 0)
        return false;

    // Recomputed each tick: entering water or jumping changes the speed.
    m.needed = miningTicks(looked_at, tool, in_water, on_ground);
    if(m.needed == MINING_NEVER)
        return false;

    ++m.progress;
    if(m.progress < m.needed)
        return false;

    m.active = false;
    m.progress = 0;
    m.cooldown = MINING_COOLDOWN_TICKS + 1;   // the next tick counts it down first
    return true;
}

int crackStage(const MiningState &m)
{
    if(!m.active || m.progress == 0 || m.needed <= 0)
        return -1;
    const int s = m.progress * 10 / m.needed;
    return s > 9 ? 9 : s;
}
