#include "crafting.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr ItemId PLANKS_A = BLOCK_PLANKS_NORMAL, PLANKS_B = BLOCK_PLANKS_DARK, PLANKS_C = BLOCK_PLANKS_BRIGHT;

constexpr Ingredient one(ItemId id, uint8_t n) { return Ingredient{ { id, 0, 0 }, n }; }
constexpr Ingredient planks(uint8_t n) { return Ingredient{ { PLANKS_A, PLANKS_B, PLANKS_C }, n }; }
constexpr Ingredient coal(uint8_t n) { return Ingredient{ { ITEM_COAL, ITEM_CHARCOAL, 0 }, n }; }
constexpr Ingredient none() { return Ingredient{ { 0, 0, 0 }, 0 }; }

constexpr Recipe shaped(ItemId out, uint8_t n, Ingredient a, Ingredient b,
                        const char *r0, const char *r1 = nullptr, const char *r2 = nullptr)
{
    return Recipe{ out, n, { a, b }, { r0, r1, r2 }, false };
}
constexpr Recipe shapeless(ItemId out, uint8_t n, Ingredient a, Ingredient b, const char *items)
{
    return Recipe{ out, n, { a, b }, { items, nullptr, nullptr }, true };
}

constexpr Ingredient cobble(uint8_t n) { return one(BLOCK_COBBLESTONE, n); }
constexpr Ingredient iron(uint8_t n) { return one(ITEM_IRON_INGOT, n); }
constexpr Ingredient gold(uint8_t n) { return one(ITEM_GOLD_INGOT, n); }
constexpr Ingredient diamond(uint8_t n) { return one(ITEM_DIAMOND, n); }
constexpr Ingredient sticks(uint8_t n) { return one(ITEM_STICK, n); }

// The four tool shapes of one material.
#define TOOLS(mat, ingredient) \
    shaped(toolId(ToolMaterial::mat, ToolKind::PICKAXE), 1, ingredient(3), sticks(2), "aaa", " b ", " b "), \
    shaped(toolId(ToolMaterial::mat, ToolKind::AXE),     1, ingredient(3), sticks(2), "aa", "ab", " b"), \
    shaped(toolId(ToolMaterial::mat, ToolKind::SHOVEL),  1, ingredient(1), sticks(2), "a", "b", "b"), \
    shaped(toolId(ToolMaterial::mat, ToolKind::SWORD),   1, ingredient(2), sticks(1), "a", "a", "b")

// Storage block and back.
#define STORAGE(block, item) \
    shaped(block, 1, one(item, 9), none(), "aaa", "aaa", "aaa"), \
    shapeless(item, 9, one(block, 1), none(), "a")

const Recipe recipes[] = {
    // The 2x2 basics
    shapeless(BLOCK_PLANKS_NORMAL, 4, one(BLOCK_WOOD, 1), none(), "a"),
    shaped(ITEM_STICK, 4, planks(2), none(), "a", "a"),
    shaped(BLOCK_CRAFTING_TABLE, 1, planks(4), none(), "aa", "aa"),
    shaped(BLOCK_TORCH, 4, coal(1), sticks(1), "a", "b"),

    // Tools
    TOOLS(WOOD, planks),
    TOOLS(STONE, cobble),
    TOOLS(IRON, iron),
    TOOLS(GOLD, gold),
    TOOLS(DIAMOND, diamond),
    shaped(ITEM_BOW, 1, sticks(3), one(ITEM_STRING, 3), " ab", "a b", " ab"),

    // Blocks
    shaped(BLOCK_FURNACE, 1, cobble(8), none(), "aaa", "a a", "aaa"),
    shaped(BLOCK_CHEST, 1, planks(8), none(), "aaa", "a a", "aaa"),
    shaped(BLOCK_BED, 1, one(BLOCK_WOOL, 3), planks(3), "aaa", "bbb"),
    shaped(BLOCK_SMITHING_TABLE, 1, iron(2), planks(4), "aa", "bb", "bb"),
    shaped(BLOCK_DOOR, 3, planks(6), none(), "aa", "aa", "aa"),
    shaped(BLOCK_TNT, 1, one(ITEM_GUNPOWDER, 5), one(BLOCK_SAND, 4), "aba", "bab", "aba"),
    shaped(ITEM_BREAD, 1, one(ITEM_WHEAT, 3), none(), "aaa"),
    shaped(ITEM_BUCKET, 1, iron(3), none(), "a a", " a "),

    // Redstone
    shaped(BLOCK_REDSTONE_TORCH, 1, one(ITEM_REDSTONE_DUST, 1), sticks(1), "a", "b"),
    shaped(BLOCK_REDSTONE_SWITCH, 1, sticks(1), cobble(1), "a", "b"),
    shaped(BLOCK_PRESSURE_PLATE, 1, one(BLOCK_STONE, 2), none(), "aa"),
    shaped(BLOCK_REDSTONE_LAMP, 1, one(ITEM_REDSTONE_DUST, 4), one(BLOCK_GLOWSTONE, 1), " a ", "aba", " a "),

    // Storage blocks
    STORAGE(BLOCK_IRON, ITEM_IRON_INGOT),
    STORAGE(BLOCK_GOLD, ITEM_GOLD_INGOT),
    STORAGE(BLOCK_DIAMOND, ITEM_DIAMOND),
    STORAGE(BLOCK_EMERALD_BLOCK, ITEM_EMERALD),

    shapeless(ITEM_NETHERITE_INGOT, 1, one(ITEM_NETHERITE_SCRAP, 4), gold(4), "aaaabbbb"),
};

#undef STORAGE
#undef TOOLS

constexpr int RECIPE_COUNT = sizeof(recipes) / sizeof(recipes[0]);

void takeIngredient(PlayerInventory &inv, const Ingredient &in)
{
    unsigned left = in.count;
    for(ItemId id : in.ids)
        if(id != 0 && left > 0)
            left -= inv.remove(id, 0, left);
}

} // namespace

int recipeCount()
{
    return RECIPE_COUNT;
}

const Recipe &recipeAt(int i)
{
    return recipes[i];
}

unsigned ingredientHave(const PlayerInventory &inv, const Ingredient &in)
{
    unsigned have = 0;
    for(ItemId id : in.ids)
        if(id != 0)
            have += inv.count(id, 0);
    return have;
}

bool hasIngredients(const PlayerInventory &inv, const Recipe &r)
{
    for(const Ingredient &in : r.in)
        if(in.count > 0 && ingredientHave(inv, in) < in.count)
            return false;
    return true;
}

CraftResult craftOnce(PlayerInventory &inv, const Recipe &r)
{
    if(!hasIngredients(inv, r))
        return CraftResult::MISSING;

    // Work on a copy so a full inventory loses nothing.
    PlayerInventory after = inv;
    for(const Ingredient &in : r.in)
        if(in.count > 0)
            takeIngredient(after, in);
    if(after.add(ItemStack(r.result, r.result_count)) != 0)
        return CraftResult::NO_ROOM;

    inv = after;
    return CraftResult::OK;
}

int craftMax(PlayerInventory &inv, const Recipe &r)
{
    int made = 0;
    while(made < 64 * PlayerInventory::SLOTS && craftOnce(inv, r) == CraftResult::OK)
        ++made;
    return made;
}

int listRecipes(const PlayerInventory &inv, bool at_table, int *out, int max)
{
    int n = 0;
    for(int pass = 0; pass < 2; ++pass)
        for(int i = 0; i < RECIPE_COUNT && n < max; ++i)
        {
            const Recipe &r = recipes[i];
            if(!at_table && needsTable(r))
                continue;
            if(hasIngredients(inv, r) == (pass == 0))
                out[n++] = i;
        }
    return n;
}

const char *ingredientName(const Ingredient &in)
{
    switch(in.ids[0])
    {
    case PLANKS_A: return "Planks";
    case ITEM_COAL: return in.ids[1] == ITEM_CHARCOAL ? "Coal or Charcoal" : "Coal";
    case BLOCK_WOOD: return "Wood";
    case BLOCK_COBBLESTONE: return "Cobblestone";
    case BLOCK_STONE: return "Stone";
    case BLOCK_SAND: return "Sand";
    case BLOCK_WOOL: return "Wool";
    case BLOCK_GLOWSTONE: return "Glowstone";
    case BLOCK_IRON: return "Iron Block";
    case BLOCK_GOLD: return "Gold Block";
    case BLOCK_DIAMOND: return "Diamond Block";
    case BLOCK_EMERALD_BLOCK: return "Emerald Block";
    default:
        break;
    }
    const char *name = isBlockItem(in.ids[0]) ? nullptr : itemDef(in.ids[0]).name;
    return name ? name : "?";
}

// ---- shapes and the grid

void recipeSize(const Recipe &r, int &w, int &h)
{
    w = h = 0;
    if(r.shapeless)
    {
        // Shapeless items fill a grid row by row; report the smallest square.
        const int n = static_cast<int>(strlen(r.shape[0]));
        w = h = n <= 1 ? 1 : n <= 4 ? 2 : 3;
        return;
    }
    for(const char *row : r.shape)
        if(row)
        {
            ++h;
            w = std::max(w, static_cast<int>(strlen(row)));
        }
}

bool needsTable(const Recipe &r)
{
    int w, h;
    recipeSize(r, w, h);
    return w > 2 || h > 2;
}

bool ingredientAccepts(const Ingredient &in, const ItemStack &s)
{
    if(s.empty() || in.count == 0)
        return false;
    for(ItemId id : in.ids)
        if(id != 0 && id == s.id)
            return true;
    return false;
}

namespace {

// 'a', 'b', ' ' for an empty cell, '?' for something the recipe doesn't use.
char cellKey(const Recipe &r, const ItemStack &s)
{
    if(s.empty())
        return ' ';
    if(ingredientAccepts(r.in[0], s))
        return 'a';
    if(ingredientAccepts(r.in[1], s))
        return 'b';
    return '?';
}

char shapeAt(const Recipe &r, int row, int col)
{
    const char *line = r.shape[row];
    return col < static_cast<int>(strlen(line)) ? line[col] : ' ';
}

bool matchesShape(const Recipe &r, const ItemStack *grid, int g)
{
    if(r.shapeless)
    {
        int na = 0, nb = 0;
        for(int i = 0; i < g * g; ++i)
            switch(cellKey(r, grid[i]))
            {
            case 'a': ++na; break;
            case 'b': ++nb; break;
            case '?': return false;
            default: break;
            }
        return na == r.in[0].count && nb == r.in[1].count;
    }

    // The filled cells' bounding box must be exactly the shape's size.
    int top = g, left = g, bottom = -1, right = -1;
    for(int y = 0; y < g; ++y)
        for(int x = 0; x < g; ++x)
            if(!grid[y * g + x].empty())
            {
                top = std::min(top, y); bottom = std::max(bottom, y);
                left = std::min(left, x); right = std::max(right, x);
            }
    if(bottom < 0)
        return false;
    int w, h;
    recipeSize(r, w, h);
    if(right - left + 1 != w || bottom - top + 1 != h)
        return false;

    for(int mirror = 0; mirror < 2; ++mirror)
    {
        bool ok = true;
        for(int y = 0; y < h && ok; ++y)
            for(int x = 0; x < w && ok; ++x)
                ok = shapeAt(r, y, mirror ? w - 1 - x : x) == cellKey(r, grid[(top + y) * g + left + x]);
        if(ok)
            return true;
    }
    return false;
}

} // namespace

int matchGrid(const ItemStack *grid, int g)
{
    for(int i = 0; i < RECIPE_COUNT; ++i)
    {
        int w, h;
        recipeSize(recipes[i], w, h);
        if(w <= g && h <= g && matchesShape(recipes[i], grid, g))
            return i;
    }
    return -1;
}

void consumeGrid(ItemStack *grid, int g)
{
    for(int i = 0; i < g * g; ++i)
        if(!grid[i].empty() && --grid[i].count == 0)
            grid[i] = ItemStack();
}

bool recipeLayout(const Recipe &r, int g, uint8_t *cells)
{
    int w, h;
    recipeSize(r, w, h);
    if(w > g || h > g)
        return false;
    std::fill(cells, cells + g * g, 0);
    if(r.shapeless)
    {
        const char *items = r.shape[0];
        for(int i = 0; items[i]; ++i)
            cells[i] = items[i] == 'a' ? 1 : 2;
        return true;
    }
    for(int y = 0; y < h; ++y)
        for(int x = 0; x < w; ++x)
        {
            const char c = shapeAt(r, y, x);
            cells[y * g + x] = c == 'a' ? 1 : c == 'b' ? 2 : 0;
        }
    return true;
}

bool fillGrid(PlayerInventory &inv, ItemStack *grid, int g, const Recipe &r)
{
    uint8_t cells[9];
    if(!recipeLayout(r, g, cells))
        return false;

    // Every cell must be empty, or already hold that cell's ingredient with room for one more.
    for(int i = 0; i < g * g; ++i)
    {
        if(cells[i] == 0)
        {
            if(!grid[i].empty())
                return false;
            continue;
        }
        const ItemStack &s = grid[i];
        if(!s.empty() && (!ingredientAccepts(r.in[cells[i] - 1], s) || s.count >= itemDef(s.id).max_stack))
            return false;
    }

    PlayerInventory after = inv;
    ItemStack placed[9];
    std::copy(grid, grid + g * g, placed);
    for(int i = 0; i < g * g; ++i)
    {
        if(cells[i] == 0)
            continue;
        const Ingredient &in = r.in[cells[i] - 1];
        // Top up with the same kind already in the cell, else the first kind on hand.
        ItemId id = placed[i].empty() ? 0 : placed[i].id;
        if(id == 0)
            for(ItemId candidate : in.ids)
                if(candidate != 0 && after.count(candidate, 0) > 0)
                {
                    id = candidate;
                    break;
                }
        if(id == 0 || after.remove(id, 0, 1) != 1)
            return false;
        placed[i] = ItemStack(id, static_cast<uint8_t>(placed[i].count + 1));
    }

    inv = after;
    std::copy(placed, placed + g * g, grid);
    return true;
}
