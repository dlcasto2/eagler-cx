#include "doctest.h"
#include "gamestate.h"

TEST_CASE("difficulty table matches the spec") {
    const DifficultyRules &p = difficultyRules(Difficulty::PEACEFUL);
    CHECK(p.hostile_cap == 0); CHECK_FALSE(p.can_starve); CHECK(p.auto_refill); CHECK(p.hunger_drain_percent == 0);
    CHECK(difficultyRules(Difficulty::EASY).hostile_cap == 4); CHECK(difficultyRules(Difficulty::EASY).starvation_floor == 10);
    CHECK(difficultyRules(Difficulty::NORMAL).hostile_cap == 6); CHECK(difficultyRules(Difficulty::NORMAL).starvation_floor == 1);
    CHECK(difficultyRules(Difficulty::HARD).hostile_cap == 8); CHECK(difficultyRules(Difficulty::HARD).starvation_floor == 0);
    const DifficultyRules &t = difficultyRules(Difficulty::TORMENT);
    CHECK(t.hostile_cap == 12); CHECK(t.mob_damage_level == 3); CHECK(t.mob_damage_percent == 150);
    CHECK(t.regen_percent == 50); CHECK(t.hunger_drain_percent == 150); CHECK_FALSE(t.undead_burn); CHECK(t.locked);
}
TEST_CASE("torment is locked") {
    CHECK_FALSE(canChangeDifficulty(Difficulty::TORMENT, Difficulty::HARD, false));
    CHECK(canChangeDifficulty(Difficulty::TORMENT, Difficulty::TORMENT, false));
    CHECK(canChangeDifficulty(Difficulty::HARD, Difficulty::TORMENT, false));
}
TEST_CASE("hardcore stays at hard or torment") {
    CHECK_FALSE(canChangeDifficulty(Difficulty::HARD, Difficulty::NORMAL, true));
    CHECK(canChangeDifficulty(Difficulty::HARD, Difficulty::TORMENT, true));
    CHECK_FALSE(canChangeGameMode(true)); CHECK(canChangeGameMode(false));
}
TEST_CASE("free changes otherwise") {
    CHECK(canChangeDifficulty(Difficulty::NORMAL, Difficulty::PEACEFUL, false));
}
TEST_CASE("new state defaults") {
    PlayerState s; CHECK(s.health == 20); CHECK(s.hunger == 20); CHECK(s.saturation_x100 == 500);
    CHECK(s.air_ticks == 300); CHECK(s.mode == GameMode::CREATIVE);
    WorldState w; CHECK(w.time_of_day == 1000); CHECK(w.difficulty == Difficulty::NORMAL);
}
