#ifndef CRAFTING_H
#define CRAFTING_H

#include "playerinventory.h"

// Crafting is a recipe list, not a 3x3 grid: a keypad can't drag items.
// Recipes match on ingredient counts only, never on arrangement.

// One ingredient: `count` items taken from any mix of `ids` (0 ends the
// list), so "4 planks" accepts all three plank kinds.
struct Ingredient {
    ItemId ids[3];
    uint8_t count;
};

struct Recipe {
    ItemId result;
    uint8_t result_count;
    Ingredient in[2];       // count 0 = unused
    bool needs_table;       // false: also on the inventory screen (Minecraft's 2x2 grid)
};

constexpr int MAX_RECIPES = 64;   // room in the screens' lists

int recipeCount();
const Recipe &recipeAt(int i);

// How many of an ingredient's items the inventory holds.
unsigned ingredientHave(const PlayerInventory &inv, const Ingredient &in);
bool hasIngredients(const PlayerInventory &inv, const Recipe &r);

enum class CraftResult { OK, MISSING, NO_ROOM };

// Takes the ingredients and adds the result, or changes nothing.
// NO_ROOM: the result wouldn't fit even after the ingredients are gone.
CraftResult craftOnce(PlayerInventory &inv, const Recipe &r);
// Crafts until something runs out. Returns how many times it crafted.
int craftMax(PlayerInventory &inv, const Recipe &r);

// Fills `out` with the recipe indices shown on a screen, craftable ones
// first, each group in table order. Returns how many it wrote.
int listRecipes(const PlayerInventory &inv, bool at_table, int *out, int max);

// The ingredient's display name ("Planks", "Coal", "Iron Ingot").
const char *ingredientName(const Ingredient &in);

#endif // CRAFTING_H
