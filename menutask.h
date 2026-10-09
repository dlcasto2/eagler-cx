#ifndef MENUTASK_H
#define MENUTASK_H

#include "gl.h"

#include "task.h"

// The game menu: the title wordmark over a stack of wide buttons.
class MenuTask : public Task
{
public:
    enum MENUITEM {
        NEW_WORLD = 0,
        LOAD_WORLD,
        SAVE_WORLD,
        SETTINGS,
        EXIT,
        HELP,
        MENU_ITEM_MAX
    };

    virtual void makeCurrent() override;

    virtual void render() override;
    virtual void logic() override;

    int menu_selected_item = SAVE_WORLD;
};

extern MenuTask menu_task;

#endif // MENUTASK_H
