#ifndef ENTITY_H
#define ENTITY_H

#include <cstdint>

#include "items.h"
#include "physics.h"

enum class EntityType : uint8_t { NONE, DROPPED_ITEM, ITEM_PILE, ARROW, MOB, STRESS_DUMMY };

// One moving thing. Fields unused by a type stay at their defaults.
struct Entity {
    EntityType type = EntityType::NONE;
    AABB box;
    GLFix vx = 0, vy = 0, vz = 0;
    int16_t health = 0;
    uint16_t age_ticks = 0;
    uint8_t invulnerable_ticks = 0;
    bool on_ground = false;
    ItemStack item;
    uint8_t pile_index = 0;
    uint8_t mob_kind = 0;
};

// Fixed storage: never more than CAPACITY moving things, no heap use.
class EntityPool
{
public:
    static constexpr int CAPACITY = 32;

    // nullptr when the pool is full.
    Entity *spawn(EntityType type, const AABB &box);
    void remove(Entity *e);
    int count() const;

    // Ages everything, despawns old drops and piles, applies gravity and collision.
    void tick(const CollisionQuery &world);

    template<typename F> void forEach(F f)
    {
        for(Entity &e : entities)
            if(e.type != EntityType::NONE)
                f(e);
    }

    template<typename F> void forEach(F f) const
    {
        for(const Entity &e : entities)
            if(e.type != EntityType::NONE)
                f(e);
    }

private:
    Entity entities[CAPACITY];
};

extern EntityPool entity_pool;

void renderEntities(const EntityPool &pool);

#endif // ENTITY_H
