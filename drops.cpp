#include "drops.h"

#include "terrain.h"

namespace {

constexpr int DROP_HALF = 16;          // a drop is a quarter block wide
constexpr int MERGE_RANGE = BLOCK_SIZE;

GLFix absDiff(GLFix a, GLFix b)
{
    return a > b ? a - b : b - a;
}

GLFix centerX(const AABB &b) { return (b.low_x + b.high_x) / 2; }
GLFix centerY(const AABB &b) { return (b.low_y + b.high_y) / 2; }
GLFix centerZ(const AABB &b) { return (b.low_z + b.high_z) / 2; }

} // namespace

int countDrops(const EntityPool &pool)
{
    int n = 0;
    pool.forEach([&n](const Entity &e) { if(e.type == EntityType::DROPPED_ITEM) ++n; });
    return n;
}

bool spawnDrop(EntityPool &pool, GLFix cx, GLFix cy, GLFix cz, ItemStack s, PlayerInventory &overflow)
{
    if(s.empty())
        return true;

    // Top up a matching drop close by first.
    pool.forEach([&](Entity &e) {
        if(s.empty() || e.type != EntityType::DROPPED_ITEM || !e.item.stacksWith(s))
            return;
        if(absDiff(centerX(e.box), cx) > GLFix(MERGE_RANGE) || absDiff(centerY(e.box), cy) > GLFix(MERGE_RANGE)
           || absDiff(centerZ(e.box), cz) > GLFix(MERGE_RANGE))
            return;
        const unsigned room = itemDef(s.id).max_stack - e.item.count;
        const unsigned moving = room < s.count ? room : s.count;
        e.item.count = static_cast<uint8_t>(e.item.count + moving);
        s.count = static_cast<uint8_t>(s.count - moving);
    });
    if(s.empty())
        return true;

    if(countDrops(pool) < MAX_DROPS)
    {
        const AABB box(cx - DROP_HALF, cy - DROP_HALF, cz - DROP_HALF, cx + DROP_HALF, cy + DROP_HALF, cz + DROP_HALF);
        if(Entity *e = pool.spawn(EntityType::DROPPED_ITEM, box))
        {
            e->item = s;
            e->vy = 6;   // a small hop
            return true;
        }
    }

    return overflow.add(s) == 0;
}

void collectDrops(EntityPool &pool, const AABB &player, PlayerInventory &inv)
{
    // 1.8.8 reach (the player's box grown by a block sideways, half a block up),
    // but a full block down, so a drop in the hole you just dug next to you is
    // collected without stepping into it.
    AABB reach(player.low_x - BLOCK_SIZE, player.low_y - BLOCK_SIZE, player.low_z - BLOCK_SIZE,
                     player.high_x + BLOCK_SIZE, player.high_y + BLOCK_SIZE / 2, player.high_z + BLOCK_SIZE);

    pool.forEach([&](Entity &e) {
        if(e.type != EntityType::DROPPED_ITEM || e.age_ticks < PICKUP_DELAY_TICKS || !reach.intersects(e.box))
            return;
        const unsigned left = inv.add(e.item);
        if(left == 0)
            pool.remove(&e);
        else
            e.item.count = static_cast<uint8_t>(left);
    });
}
