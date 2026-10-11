#include "smithingscreen.h"

#include "containers.h"
#include "drops.h"
#include "entity.h"
#include "inventoryscreen.h"
#include "playerinventory.h"
#include "screenmove.h"
#include "uikit.h"
#include "worldtask.h"

SmithingScreen smithing_screen;

constexpr int SmithingScreen::PANEL_W, SmithingScreen::PANEL_H;

namespace {
const int top_slot_x[3] = { 26, 75, 133 };   // tool, ingot, result
constexpr int TOP_Y = 40;
}

void SmithingScreen::makeCurrent()
{
    if(!background_saved)
        saveBackground();
    held = inputs[0] = inputs[1] = ItemStack();
    cursor_row = 1;
    cursor_col = 0;
    Task::makeCurrent();
}

void SmithingScreen::close()
{
    for(ItemStack *s : { &held, &inputs[0], &inputs[1] })
    {
        if(!s->empty())
            returnHeld(player_inventory.slots, PlayerInventory::SLOTS, *s);
        if(!s->empty())
            spawnDrop(entity_pool, world_task.x, world_task.y + BLOCK_SIZE, world_task.z, *s, player_inventory);
        *s = ItemStack();
    }
    world_task.makeCurrent();
}

void SmithingScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    const int px = (SCREEN_WIDTH - PANEL_W) / 2, py = (SCREEN_HEIGHT - PANEL_H) / 2 - 4;
    drawPanel(*screen, px, py, PANEL_W, PANEL_H);
    drawPixelText(*screen, "Upgrade Gear", px + 8, py + 6, ui::DARK_TEXT, false);
    drawPixelText(*screen, "Inventory", px + 8, py + 73, ui::DARK_TEXT, false);

    const ItemStack result = smithingResult(inputs[0], inputs[1]);
    const ItemStack *top[3] = { &inputs[0], &inputs[1], &result };
    for(int i = 0; i < 3; ++i)
    {
        drawSlot(*screen, px + top_slot_x[i], py + TOP_Y);
        drawItemStack(*screen, px + top_slot_x[i] + 1, py + TOP_Y + 1, *top[i]);
    }
    drawPixelText(*screen, "+", px + 56, py + TOP_Y + 5, ui::DARK_TEXT, false);
    drawPixelText(*screen, "->", px + 104, py + TOP_Y + 5, ui::DARK_TEXT, false);
    drawPixelText(*screen, "Diamond tool + netherite", px + 8, py + 24, ui::GREY_TEXT - 0x2104, false);

    const int gx = px + 7;
    drawPlayerSlots(*screen, gx, py + 83);

    int sx, sy;
    const ItemStack *under;
    if(cursor_row == 0)
    {
        sx = px + top_slot_x[cursor_col];
        sy = py + TOP_Y;
        under = top[cursor_col];
    }
    else
    {
        const int i = cursor_row < 4 ? 9 + (cursor_row - 1) * 9 + cursor_col : cursor_col;
        playerSlotPos(i, gx, py + 83, sx, sy);
        under = &player_inventory.slots[i];
    }
    drawSlotHighlight(*screen, sx, sy);
    drawItemStack(*screen, sx + 1, sy + 1, *under);

    if(!held.empty())
        drawItemStack(*screen, sx + 7, sy - 6, held);
    else if(!under->empty())
        drawTooltip(*screen, itemName(*under), sx + 14, sy - 14);

    drawPixelTextCenter(*screen, "5 take/put   7 half/one   esc close", SCREEN_WIDTH / 2, py + PANEL_H + 3, ui::TEXT);
}

void SmithingScreen::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8)
                || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_7)
                || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN) || keyPressed(KEY_NSPIRE_LEFT)
                || keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    key_held_down = true;
    const int cols = cursor_row == 0 ? 3 : 9;

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
        close();
    else if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
    {
        const int old_row = cursor_row;
        cursor_row = (keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP)) ? (cursor_row + 4) % 5 : (cursor_row + 1) % 5;
        if(old_row == 0 && cursor_row != 0)
            cursor_col = top_slot_x[cursor_col] / SLOT;         // keep roughly the same column
        else if(old_row != 0 && cursor_row == 0)
            cursor_col = cursor_col < 3 ? 0 : (cursor_col < 6 ? 1 : 2);
    }
    else if(keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_LEFT))
        cursor_col = (cursor_col + cols - 1) % cols;
    else if(keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_RIGHT))
        cursor_col = (cursor_col + 1) % cols;
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK) || keyPressed(KEY_NSPIRE_7))
    {
        const bool half = keyPressed(KEY_NSPIRE_7);
        if(cursor_row == 0 && cursor_col == 2)
        {
            // Taking the result uses up the tool and one ingot.
            const ItemStack result = smithingResult(inputs[0], inputs[1]);
            if(!result.empty() && held.empty())
            {
                held = result;
                inputs[0] = ItemStack();
                if(--inputs[1].count == 0)
                    inputs[1] = ItemStack();
            }
        }
        else
        {
            ItemStack *slots = cursor_row == 0 ? inputs : player_inventory.slots;
            const int n = cursor_row == 0 ? 2 : PlayerInventory::SLOTS;
            const int i = cursor_row == 0 ? cursor_col
                                          : (cursor_row < 4 ? 9 + (cursor_row - 1) * 9 + cursor_col : cursor_col);
            if(!half)
                moveTake(slots, n, i, held);
            else if(held.empty())
                moveHalf(slots, n, i, held);
            else
                movePlaceOne(slots, n, i, held);
        }
    }
    else
        key_held_down = false;
}
