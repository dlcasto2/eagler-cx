#include "doctest.h"
#include "entity.h"
#include "grid_world.h"

TEST_CASE("33rd spawn returns null") {
    EntityPool p;
    for (int i = 0; i < 32; ++i) CHECK(p.spawn(EntityType::STRESS_DUMMY, AABB()) != nullptr);
    CHECK(p.spawn(EntityType::STRESS_DUMMY, AABB()) == nullptr); CHECK(p.count() == 32);
}
TEST_CASE("removed slots are reused") {
    EntityPool p; Entity *a = p.spawn(EntityType::MOB, AABB()); p.remove(a);
    CHECK(p.count() == 0); CHECK(p.spawn(EntityType::MOB, AABB()) == a);
}
TEST_CASE("tick applies gravity until the ground") {
    GridWorld w; w.solid.insert(std::make_tuple(0, 0, 0)); EntityPool p;
    Entity *e = p.spawn(EntityType::STRESS_DUMMY, playerBoxAt(64, 384, 64));
    for (int i = 0; i < 40; ++i) p.tick(w);
    CHECK(e->on_ground); CHECK(e->vy == GLFix(0));
}
TEST_CASE("dropped items despawn after 6000 ticks") {
    GridWorld w; EntityPool p; Entity *e = p.spawn(EntityType::DROPPED_ITEM, AABB());
    e->age_ticks = 5998; p.tick(w); CHECK(p.count() == 1); p.tick(w); CHECK(p.count() == 0);
}
