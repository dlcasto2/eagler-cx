#ifndef INVENTORYSCREEN_H
#define INVENTORYSCREEN_H

#include "task.h"
#include "items.h"

// The survival inventory: all 36 slots, a cursor and the stack in hand.
// 8 4 6 2 move, 5 take/put, 7 half/one, Enter sends across, Esc or . closes.
class InventoryScreen : public Task
{
public:
    virtual void makeCurrent() override;
    virtual void render() override;
    virtual void logic() override;

    // Puts anything held back into the inventory and returns to the world.
    void close();

    static constexpr int PANEL_W = 176, PANEL_H = 103;

    int cursor_row = 3, cursor_col = 0;   // rows 0-2 storage, row 3 hotbar
    ItemStack held;

    // The slot index under the cursor (0-8 hotbar, 9-35 storage).
    int cursorSlot() const;
};

// Draws the 3x9 storage grid at (x, y) and the hotbar row 58 pixels below,
// as every screen with the player's inventory shows it.
void drawPlayerSlots(TEXTURE &dst, int x, int y);
// Top-left corner of slot i in that layout.
void playerSlotPos(int i, int x, int y, int &sx, int &sy);

extern InventoryScreen inventory_screen;

#endif // INVENTORYSCREEN_H
