#ifndef CHESTSCREEN_H
#define CHESTSCREEN_H

#include "task.h"
#include "items.h"

// A chest's 27 slots on top, your 36 below (1.8.8's 176x166 layout).
// 8 4 6 2 move, 5 take/put, 7 half/one, Enter sends across, Esc or . closes.
class ChestScreen : public Task
{
public:
    void open(int x, int y, int z);

    virtual void makeCurrent() override;
    virtual void render() override;
    virtual void logic() override;

    void close();

    static constexpr int PANEL_W = 176, PANEL_H = 166;

    int cursor_row = 3, cursor_col = 0;   // rows 0-2 chest, 3-5 storage, 6 hotbar
    ItemStack held;

private:
    int cx = 0, cy = 0, cz = 0;
    ItemStack *chestSlots();   // nullptr if the chest is gone
};

extern ChestScreen chest_screen;

#endif // CHESTSCREEN_H
