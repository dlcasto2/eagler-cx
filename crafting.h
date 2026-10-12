#ifndef CRAFTING_H
#define CRAFTING_H

#include "playerinventory.h"

// Two ways to craft:
// - the recipe list, matched on ingredient counts only (a keypad can't drag
//   items, so this is the quick way), and
// - the crafting grid, matched on Minecraft's shapes: 2x2 in the inventory,
//   3x3 at a crafting table.

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
    // Grid rows: 'a' is in[0], 'b' is in[1], ' ' is empty; nullptr ends.
    // A shaped recipe may sit anywhere in the grid and may be mirrored.
    const char *shape[3];
    bool shapeless;         // the shape just lists the items; any arrangement
};

constexpr int MAX_RECIPES = 64;   // room in the screens' lists

int recipeCount();
const Recipe &recipeAt(int i);

// Whether it needs a crafting table: it doesn't fit Minecraft's 2x2 grid.
bool needsTable(const Recipe &r);
// The shape's size in cells.
void recipeSize(const Recipe &r, int &w, int &h);

// How many of an ingredient's items the inventory holds.
unsigned ingredientHave(const PlayerInventory &inv, const Ingredient &in);
bool hasIngredients(const PlayerInventory &inv, const Recipe &r);
bool ingredientAccepts(const Ingredient &in, const ItemStack &s);

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

// ---- the grid: g x g stacks, row by row (g = 2 or 3)

// The recipe the grid's contents make, or -1.
int matchGrid(const ItemStack *grid, int g);
// Uses up one item from every filled cell (taking the result).
void consumeGrid(ItemStack *grid, int g);
// Where a recipe's items go in an empty grid (top left, as Minecraft's recipe
// book lays it out): 0 empty, 1 in[0], 2 in[1]. False if it doesn't fit.
bool recipeLayout(const Recipe &r, int g, uint8_t *cells);
// Moves one set of the recipe's ingredients from the inventory into the grid
// on that layout, on top of a matching set already there. False (changing
// nothing) if the grid holds something else or the inventory runs short.
bool fillGrid(PlayerInventory &inv, ItemStack *grid, int g, const Recipe &r);

#endif // CRAFTING_H
