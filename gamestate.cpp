#include "gamestate.h"

PlayerState player_state;
WorldState world_state;

const DifficultyRules &difficultyRules(Difficulty d)
{
    // cap, dmg level, dmg %, can starve, floor, regen %, auto refill, drain %, undead burn, locked
    static const DifficultyRules rules[5] = {
        {  0, 0,   0, false,  0, 100, true,    0, false, false }, // Peaceful
        {  4, 1, 100, true,  10, 100, false, 100, true,  false }, // Easy
        {  6, 2, 100, true,   1, 100, false, 100, true,  false }, // Normal
        {  8, 3, 100, true,   0, 100, false, 100, true,  false }, // Hard
        { 12, 3, 150, true,   0,  50, false, 150, false, true  }, // Torment
    };
    return rules[static_cast<int>(d)];
}

bool canChangeDifficulty(Difficulty from, Difficulty to, bool hardcore)
{
    if(from == to)
        return true;                                   // no-op always allowed
    if(difficultyRules(from).locked)
        return false;                                  // never leave a locked level
    if(hardcore && to != Difficulty::HARD && to != Difficulty::TORMENT)
        return false;                                  // hardcore stays hard or above
    return true;
}

bool canChangeGameMode(bool hardcore)
{
    return !hardcore;
}
