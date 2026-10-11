#ifndef INVENTORYSCREEN_H
#define INVENTORYSCREEN_H

#include "task.h"
#include "items.h"
#include "recipelist.h"

// The survival inventory: all 36 slots, a cursor, the stack in hand and a
// recipe list on the left. The inventory offers the simple recipes; a
// crafting table opens the same screen with all of them.
// Slots: 8 4 6 2 move, 5 take/put, 7 half/one, Enter sends across.
// Recipes: 8 2 pick, 5 crafts one (hold to repeat), Enter crafts all.
// Tab (or 4 / 6 at the edge) switches between them; Esc or . closes.
class InventoryScreen : public Task
{
public:
    virtual void makeCurrent() override;   // the inventory
    virtual void render() override;
    virtual void logic() override;

    // Opens the crafting table's full recipe list.
    void openCrafting();

    // Puts anything held back into the inventory and returns to the world.
    void close();

    static constexpr int PANEL_W = 176, PANEL_H = 103, TABLE_PANEL_H = 166, LIST_W = 132;

    int cursor_row = 3, cursor_col = 0;   // rows 0-2 storage, row 3 hotbar
    ItemStack held;
    bool at_table = false, focus_list = false;
    RecipeList recipes;

    // The slot index under the cursor (0-8 hotbar, 9-35 storage).
    int cursorSlot() const;

private:
    void open(bool table);
    void craftSelected(bool all);

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
