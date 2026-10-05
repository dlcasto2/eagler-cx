#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <cstdint>

enum class GameMode : uint8_t { SURVIVAL = 0, CREATIVE = 1 };
enum class Difficulty : uint8_t { PEACEFUL = 0, EASY, NORMAL, HARD, TORMENT };

struct DifficultyRules {
    uint8_t hostile_cap;
    uint8_t mob_damage_level;    // 0 none, 1 Easy, 2 Normal, 3 Hard
    uint8_t mob_damage_percent;  // scales the chosen level
    bool can_starve;
    uint8_t starvation_floor;    // health that starvation can't go below
    uint8_t regen_percent;       // natural regen speed
    bool auto_refill;            // Peaceful refills health and hunger
    uint8_t hunger_drain_percent;
    bool undead_burn;            // zombies and skeletons burn in daylight
    bool locked;                 // can never be changed away from
};

const DifficultyRules &difficultyRules(Difficulty d);
bool canChangeDifficulty(Difficulty from, Difficulty to, bool hardcore);
bool canChangeGameMode(bool hardcore);

struct PlayerState {
    uint8_t health = 20, hunger = 20;
    uint16_t saturation_x100 = 500, exhaustion_x1000 = 0, air_ticks = 300, burning_ticks = 0;
    GameMode mode = GameMode::CREATIVE;
    int32_t spawn_x = 0, spawn_y = 0, spawn_z = 0;
    bool spawn_is_bed = false;
};

struct WorldState {
    uint32_t time_of_day = 1000;
    Difficulty difficulty = Difficulty::NORMAL;
    bool hardcore = false, hardcore_dead = false;
    uint32_t trader_next_visit = 0;
};

extern PlayerState player_state;
extern WorldState world_state;

#endif // GAMESTATE_H
