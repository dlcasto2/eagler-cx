#include "helptask.h"

#include "uikit.h"
#include "worldtask.h"

HelpTask help_task;

namespace {

const char *const controls[][2] = {
    { "8 4 6 2", "Walk (auto-jump climbs steps)" },
    { "5", "Jump" },
    { "7", "Place block" },
    { "9", "Break block" },
    { "1 / 3", "Previous / next hotbar slot" },
    { ".", "Inventory (creative: block list)" },
    { "Menu", "Game menu" },
    { "Esc", "Save and quit" },
    { "+ / -", "View distance" },
    { "Ctrl + .", "Screenshot" },
    { "In screens", "5 take/put, 7 half/one, enter send" },
};

} // namespace

void HelpTask::makeCurrent()
{
    if(!background_saved)
        saveBackground();

    Task::makeCurrent();
}

void HelpTask::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    drawPixelTextCenter(*screen, "Help & Controls", SCREEN_WIDTH / 2, 8, ui::TEXT_WHITE);

    int y = 26;
    for(const auto &row : controls)
    {
        drawPixelText(*screen, row[0], 104 - pixelTextWidth(row[0]), y, ui::YELLOW);
        drawPixelText(*screen, row[1], 114, y, ui::TEXT);
        y += 13;
    }

    drawPixelTextCenter(*screen, "Crafti by Fabian Vogt - Survival Edition fork", SCREEN_WIDTH / 2, y + 4, ui::GREY_TEXT, false);
    drawPixelTextCenter(*screen, "Block textures: PureBDcraft (bdcraft.net)", SCREEN_WIDTH / 2, y + 14, ui::GREY_TEXT, false);

    drawButton(*screen, (SCREEN_WIDTH - 200) / 2, SCREEN_HEIGHT - 26, 200, 20, "Done", true);
}

void HelpTask::logic()
{
    if(key_held_down)
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK);
    else if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
    {
        world_task.makeCurrent();

        key_held_down = true;
    }
}
