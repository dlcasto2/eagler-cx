#include "chestscreen.h"

#include "blockstorage.h"
#include "drops.h"
#include "entity.h"
#include "inventoryscreen.h"
#include "playerinventory.h"
#include "screenmove.h"
#include "uikit.h"
#include "worldtask.h"

ChestScreen chest_screen;

constexpr int ChestScreen::PANEL_W, ChestScreen::PANEL_H;

ItemStack *ChestScreen::chestSlots()
{
    StorageRecord *r = block_storage.find(BlockPos{cx, cy, cz});
    if(!r || r->kind != StorageKind::CHEST || r->slots.size() < CHEST_SLOTS)
        return nullptr;
    return r->slots.data();
}

void ChestScreen::open(int x, int y, int z)
{
    cx = x; cy = y; cz = z;
    // A chest placed before Survival Edition, or in creative, has no record yet.
    if(!block_storage.find(BlockPos{x, y, z}))
        block_storage.create(BlockPos{x, y, z}, StorageKind::CHEST);
    makeCurrent();
}

void ChestScreen::makeCurrent()
{
    if(!background_saved)
        saveBackground();
    held = ItemStack();
    cursor_row = 0;
    cursor_col = 0;
    Task::makeCurrent();
}

void ChestScreen::close()
{
    // Whatever is in hand goes back: inventory first, then the chest, then the ground.
    if(!held.empty())
        returnHeld(player_inventory.slots, PlayerInventory::SLOTS, held);
    if(!held.empty())
        if(ItemStack *chest = chestSlots())
            returnHeld(chest, CHEST_SLOTS, held);
    if(!held.empty())
        spawnDrop(entity_pool, world_task.x, world_task.y + BLOCK_SIZE, world_task.z, held, player_inventory);
    held = ItemStack();
    world_task.makeCurrent();
}

void ChestScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    ItemStack *chest = chestSlots();
    const int px = (SCREEN_WIDTH - PANEL_W) / 2, py = (SCREEN_HEIGHT - PANEL_H) / 2 - 4;
    drawPanel(*screen, px, py, PANEL_W, PANEL_H);
    drawPixelText(*screen, "Chest", px + 8, py + 6, ui::DARK_TEXT, false);
    drawPixelText(*screen, "Inventory", px + 8, py + 73, ui::DARK_TEXT, false);

    const int gx = px + 7;
    for(int i = 0; i < 27; ++i)
    {
        const int sx = gx + SLOT * (i % 9), sy = py + 17 + SLOT * (i / 9);
        drawSlot(*screen, sx, sy);
        if(chest)
            drawItemStack(*screen, sx + 1, sy + 1, chest[i]);
    }
    drawPlayerSlots(*screen, gx, py + 83);

    // cursor
    int sx, sy;
    const ItemStack *under = nullptr;
    if(cursor_row < 3)
    {
        sx = gx + SLOT * cursor_col;
        sy = py + 17 + SLOT * cursor_row;
        if(chest) under = &chest[cursor_row * 9 + cursor_col];
    }
    else
    {
        const int i = cursor_row < 6 ? 9 + (cursor_row - 3) * 9 + cursor_col : cursor_col;
        playerSlotPos(i, gx, py + 83, sx, sy);
        under = &player_inventory.slots[i];
    }
    drawSlotHighlight(*screen, sx, sy);
    if(under)
        drawItemStack(*screen, sx + 1, sy + 1, *under);

    if(!held.empty())
        drawItemStack(*screen, sx + 7, sy - 6, held);
    else if(under && !under->empty())
        drawTooltip(*screen, itemName(*under), sx + 14, sy - 14);

    drawPixelTextCenter(*screen, "5 take/put   7 half/one   enter send   esc close", SCREEN_WIDTH / 2, py + PANEL_H + 3, ui::TEXT);
}

void ChestScreen::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8)
                || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_7)
                || keyPressed(KEY_NSPIRE_ENTER) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN)
                || keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    ItemStack *chest = chestSlots();
    if(!chest)   // the chest is gone
    {
        close();
        return;
    }

    key_held_down = true;
    ItemStack *inv = player_inventory.slots;
    const bool in_chest = cursor_row < 3;
    ItemStack *slots = in_chest ? chest : inv;
    const int n = in_chest ? CHEST_SLOTS : PlayerInventory::SLOTS;
    const int i = in_chest ? cursor_row * 9 + cursor_col
                           : (cursor_row < 6 ? 9 + (cursor_row - 3) * 9 + cursor_col : cursor_col);

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
        close();
    else if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
        cursor_row = (cursor_row + 6) % 7;
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
        cursor_row = (cursor_row + 1) % 7;
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
        if(in_chest)
            moveSendAcross(chest, CHEST_SLOTS, i, inv, PlayerInventory::SLOTS);
        else
            moveSendAcross(inv, PlayerInventory::SLOTS, i, chest, CHEST_SLOTS);
    }
    else
        key_held_down = false;
}
