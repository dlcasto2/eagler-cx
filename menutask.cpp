#include "menutask.h"

#include "gui_art.h"
#include "helptask.h"
#include "settingstask.h"
#include "uikit.h"
#include "worldtask.h"

MenuTask menu_task;

namespace {

const char *const labels[MenuTask::MENU_ITEM_MAX] = {
    "New World", "Load World", "Save World", "Options...", "Quit without Saving", "Help & Controls"
};

// Buttons in the order they're shown, top to bottom.
const int order[MenuTask::MENU_ITEM_MAX] = {
    MenuTask::SAVE_WORLD, MenuTask::LOAD_WORLD, MenuTask::NEW_WORLD, MenuTask::SETTINGS, MenuTask::HELP, MenuTask::EXIT
};

int positionOf(int item)
{
    for(int i = 0; i < MenuTask::MENU_ITEM_MAX; ++i)
        if(order[i] == item)
            return i;
    return 0;
}

constexpr int BUTTON_W = 200, BUTTON_H = 20, BUTTON_GAP = 4, BUTTONS_Y = 68;

} // namespace

void MenuTask::makeCurrent()
{
    menu_selected_item = SAVE_WORLD;

    if(!background_saved)
        saveBackground();

    Task::makeCurrent();
}

void MenuTask::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    blitKeyed(gui_title, 0, 0, gui_title.width, gui_title.height, *screen, (SCREEN_WIDTH - gui_title.width) / 2, 10);
    drawPixelTextCenter(*screen, "Survival Edition", SCREEN_WIDTH / 2, 10 + gui_title.height + 4, ui::TITLE);

    const int x = (SCREEN_WIDTH - BUTTON_W) / 2;
    for(int i = 0; i < MENU_ITEM_MAX; ++i)
        drawButton(*screen, x, BUTTONS_Y + i * (BUTTON_H + BUTTON_GAP), BUTTON_W, BUTTON_H, labels[order[i]], order[i] == menu_selected_item);

    drawPixelText(*screen, "8/2 move   5 select   menu back", 3, SCREEN_HEIGHT - 11, ui::GREY_TEXT);
}

void MenuTask::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_CLICK) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN) || keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_MENU) || keyPressed(KEY_NSPIRE_ESC);
        return;
    }

    if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
    {
        menu_selected_item = order[(positionOf(menu_selected_item) + MENU_ITEM_MAX - 1) % MENU_ITEM_MAX];
        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
    {
        menu_selected_item = order[(positionOf(menu_selected_item) + 1) % MENU_ITEM_MAX];
        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
    {
        key_held_down = true;

        switch(menu_selected_item)
        {
        case NEW_WORLD:
            world_task.resetWorld();
            world_task.makeCurrent();
            break;

        case LOAD_WORLD:
            if(load() == LoadResult::OK)
                world_task.setMessage("World loaded.");
            else
                world_task.setMessage("World failed to load.");
            world_task.makeCurrent();
            break;

        case SAVE_WORLD:
            if(save())
                world_task.setMessage("World saved.");
            else
                world_task.setMessage("Failed to save world.");
            world_task.makeCurrent();
            break;

        case EXIT:
            running = false;
            break;

        case HELP:
            help_task.makeCurrent();
            break;

        case SETTINGS:
            settings_task.makeCurrent();
            break;
        }
    }
    else if(keyPressed(KEY_NSPIRE_MENU) || keyPressed(KEY_NSPIRE_ESC))
    {
        world_task.makeCurrent();
        key_held_down = true;
    }
}
