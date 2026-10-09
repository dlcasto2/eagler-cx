#include "inventoryscreen.h"

#include "playerinventory.h"
#include "screenmove.h"
#include "uikit.h"
#include "worldtask.h"

InventoryScreen inventory_screen;

constexpr int InventoryScreen::PANEL_W, InventoryScreen::PANEL_H;

void playerSlotPos(int i, int x, int y, int &sx, int &sy)
{
    if(i < PlayerInventory::HOTBAR)
    {
        sx = x + SLOT * i;
        sy = y + 3 * SLOT + 4;
    }
    else
    {
        sx = x + SLOT * ((i - 9) % 9);
        sy = y + SLOT * ((i - 9) / 9);
    }
}

void drawPlayerSlots(TEXTURE &dst, int x, int y)
{
    for(int i = 0; i < PlayerInventory::SLOTS; ++i)
    {
        int sx, sy;
        playerSlotPos(i, x, y, sx, sy);
        drawSlot(dst, sx, sy);
        drawItemStack(dst, sx + 1, sy + 1, player_inventory.slots[i]);
    }
}

int InventoryScreen::cursorSlot() const
{
    return cursor_row < 3 ? 9 + cursor_row * 9 + cursor_col : cursor_col;
}

void InventoryScreen::makeCurrent()
{
    if(!background_saved)
        saveBackground();

    held = ItemStack();
    cursor_row = 3;
    cursor_col = player_inventory.selected;

    Task::makeCurrent();
}

void InventoryScreen::close()
{
    returnHeld(player_inventory.slots, PlayerInventory::SLOTS, held);
    world_task.makeCurrent();
}

void InventoryScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    const int px = (SCREEN_WIDTH - PANEL_W) / 2, py = (SCREEN_HEIGHT - PANEL_H) / 2 - 6;
    drawPanel(*screen, px, py, PANEL_W, PANEL_H);
    drawPixelText(*screen, "Inventory", px + 8, py + 6, ui::DARK_TEXT, false);

    const int gx = px + 7, gy = py + 18;
    drawPlayerSlots(*screen, gx, gy);

    int sx, sy;
    playerSlotPos(cursorSlot(), gx, gy, sx, sy);
    drawSlotHighlight(*screen, sx, sy);
    drawItemStack(*screen, sx + 1, sy + 1, player_inventory.slots[cursorSlot()]);

    if(!held.empty())
        drawItemStack(*screen, sx + 7, sy - 6, held);   // the stack in hand floats over the cursor
    else
    {
        const char *name = itemName(player_inventory.slots[cursorSlot()]);
        if(*name)
            drawTooltip(*screen, name, sx + 14, sy - 14);
    }

    drawPixelTextCenter(*screen, "5 take/put   7 half/one   enter send   esc close", SCREEN_WIDTH / 2, py + PANEL_H + 4, ui::TEXT);
}

void InventoryScreen::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8)
                || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_7)
                || keyPressed(KEY_NSPIRE_ENTER) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN)
                || keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    key_held_down = true;
    ItemStack *slots = player_inventory.slots;
    const int n = PlayerInventory::SLOTS, i = cursorSlot();

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
        close();
    else if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
        cursor_row = (cursor_row + 3) % 4;
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
        cursor_row = (cursor_row + 1) % 4;
    else if(keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_LEFT))
        cursor_col = (cursor_col + 8) % 9;
    else if(keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_RIGHT))
        cursor_col = (cursor_col + 1) % 9;
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
        moveTake(slots, n, i, held);
    else if(keyPressed(KEY_NSPIRE_7))
    {
        if(held.empty())
            moveHalf(slots, n, i, held);
        else
            movePlaceOne(slots, n, i, held);
    }
    else if(keyPressed(KEY_NSPIRE_ENTER))
    {
        if(i < PlayerInventory::HOTBAR)
            moveSendAcross(slots, PlayerInventory::HOTBAR, i, slots + PlayerInventory::HOTBAR, n - PlayerInventory::HOTBAR);
        else
            moveSendAcross(slots + PlayerInventory::HOTBAR, n - PlayerInventory::HOTBAR, i - PlayerInventory::HOTBAR, slots, PlayerInventory::HOTBAR);
    }
    else
        key_held_down = false;
}
