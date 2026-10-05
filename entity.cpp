#include "entity.h"

constexpr int EntityPool::CAPACITY;

EntityPool entity_pool;

Entity *EntityPool::spawn(EntityType type, const AABB &box)
{
    for(Entity &e : entities)
        if(e.type == EntityType::NONE)
        {
            e = Entity();
            e.type = type;
            e.box = box;
            return &e;
        }

    return nullptr;
}

void EntityPool::remove(Entity *e)
{
    e->type = EntityType::NONE;
}

int EntityPool::count() const
{
    int n = 0;
    for(const Entity &e : entities)
        if(e.type != EntityType::NONE)
            ++n;
    return n;
}

void EntityPool::tick(const CollisionQuery &world)
{
    for(Entity &e : entities)
    {
        if(e.type == EntityType::NONE)
            continue;

        ++e.age_ticks;
        if((e.type == EntityType::DROPPED_ITEM || e.type == EntityType::ITEM_PILE) && e.age_ticks >= DESPAWN_TICKS)
        {
            remove(&e);
            continue;
        }

        e.vy -= GRAVITY_PER_TICK;
        const MoveResult r = moveWithCollision(world, e.box, e.vx, e.vy, e.vz);
        if(r.blocked_y)
            e.vy = 0;
        e.on_ground = r.on_ground;
    }
}
