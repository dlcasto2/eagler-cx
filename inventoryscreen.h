#ifndef INVENTORYSCREEN_H
#define INVENTORYSCREEN_H

#include "task.h"
#include "items.h"
#include "recipelist.h"

// The survival inventory with a 2x2 crafting grid, or a crafting table's
// 3x3 grid, beside a recipe list.
// Slots and grid: 8 4 6 2 move, 5 take/put, 7 half/one, Enter sends across.
// The result slot: 5 or 7 takes one craft, Enter crafts all into the inventory.
// Recipe list: 8 2 pick, 5 crafts one (hold to repeat), 7 lays it in the
// grid, Enter crafts all. The empty grid shows the picked recipe's shape.
// Tab (or 4 at the left edge, 6 from the list) switches; Esc or . closes.
class InventoryScreen : public Task
{
public:
    virtual void makeCurrent() override;   // the inventory
    virtual void render() override;
    virtual void logic() override;

    // Opens the crafting table: 3x3 grid and every recipe.
    void openCrafting();

    // Puts the grid and anything held back into the inventory and returns to the world.
    void close();

    static constexpr int PANEL_W = 176, PANEL_H = 166, LIST_W = 132;

    // Rows 0..g-1 are the grid (column g is the result), then three storage
    // rows and the hotbar.
    int cursor_row = 0, cursor_col = 0;
    ItemStack held;
    ItemStack grid[9];
    bool at_table = false, focus_list = false;
    RecipeList recipes;

    int gridSize() const { return at_table ? 3 : 2; }
    // The inventory slot under the cursor (0-8 hotbar, 9-35 storage), or -1 in the grid.
    int cursorSlot() const;

private:
    void open(bool table);
    void craftSelected(bool all);
    void takeResult(bool all);
    void fillFromList();
    void emptyGrid();
    void showMessage(const char *m);
    void moveCursorVertical(bool up);

    int hold_frames = 0;
    int message_frames = 0;
    const char *message = "";
};

// Draws the 3x9 storage grid at (x, y) and the hotbar row 58 pixels below,
// as every screen with the player's inventory shows it.
void drawPlayerSlots(TEXTURE &dst, int x, int y);
// Top-left corner of slot i in that layout.
void playerSlotPos(int i, int x, int y, int &sx, int &sy);

extern InventoryScreen inventory_screen;

#endif // INVENTORYSCREEN_H
