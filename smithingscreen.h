#ifndef SMITHINGSCREEN_H
#define SMITHINGSCREEN_H

#include "task.h"
#include "items.h"

// Diamond tool + netherite ingot -> the netherite tool, keeping its wear.
// The top row has three slots (tool, ingot, result); your inventory is below.
class SmithingScreen : public Task
{
public:
    virtual void makeCurrent() override;
    virtual void render() override;
    virtual void logic() override;

    void close();

    static constexpr int PANEL_W = 176, PANEL_H = 166;

    int cursor_row = 0, cursor_col = 0;   // row 0: tool, ingot, result; 1-3 storage; 4 hotbar
    ItemStack held, inputs[2];   // inputs[0] tool, inputs[1] ingot
};

extern SmithingScreen smithing_screen;

#endif // SMITHINGSCREEN_H
