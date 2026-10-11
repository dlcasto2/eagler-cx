#ifndef FURNACESCREEN_H
#define FURNACESCREEN_H

#include "task.h"
#include "items.h"

struct StorageRecord;

// A furnace's input, fuel and output on top, your inventory below.
// The furnace keeps cooking while this screen is open.
// 8 4 6 2 move, 5 take/put, 7 half/one, Enter sends across, Esc or . closes.
class FurnaceScreen : public Task
{
public:
    void open(int x, int y, int z);

    virtual void makeCurrent() override;
    virtual void render() override;
    virtual void logic() override;
    virtual void tick() override;

    void close();

    static constexpr int PANEL_W = 176, PANEL_H = 166;

    int cursor_row = 1, cursor_col = 0;   // row 0: input, fuel, output; 1-3 storage; 4 hotbar
    ItemStack held;

private:
    int fx = 0, fy = 0, fz = 0;
    StorageRecord *record();   // nullptr if the furnace is gone
};

extern FurnaceScreen furnace_screen;

#endif // FURNACESCREEN_H
