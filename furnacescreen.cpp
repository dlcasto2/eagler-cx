#include "furnacescreen.h"

#include "blockstorage.h"
#include "drops.h"
#include "entity.h"
#include "furnace.h"
#include "inventoryscreen.h"
#include "playerinventory.h"
#include "screenmove.h"
#include "uikit.h"
#include "worldtask.h"

FurnaceScreen furnace_screen;

constexpr int FurnaceScreen::PANEL_W, FurnaceScreen::PANEL_H;

namespace {

// Slot corners inside the panel: input, fuel, output (1.8.8's layout).
const int slot_x[3] = { 56, 56, 116 }, slot_y[3] = { 17, 53, 35 };
constexpr int FLAME_X = 57, FLAME_Y = 37, ARROW_X = 79, ARROW_Y = 35;

constexpr COLOR grey565(unsigned v) { return static_cast<COLOR>(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3)); }

// An original 14x14 flame: '#' rim, 'o' body, 'y' core.
const char *const flame[14] = {
    "......#.......",
    ".....##.......",
    ".....###......",
    "....####..#...",
    "...#####.##...",
    "...########...",
    "..####o#####..",
    "..###ooo####..",
    ".####oyoo###..",
    ".###ooyyoo###.",
    ".##ooyyyyoo##.",
    ".##ooyyyyyo##.",
    "..##ooyyoo##..",
    "...########...",
};

// Fills the bottom `lit` of 14 rows in color, the rest as a grey outline.
void drawFlame(TEXTURE &dst, int x, int y, int lit)
{
    for(int r = 0; r < 14; ++r)
        for(int c = 0; c < 14; ++c)
        {
            const char p = flame[r][c];
            if(p == '.')
                continue;
            COLOR col = grey565(0x8B);
            if(r >= 14 - lit)
                col = p == '#' ? 0xE1A0 : p == 'o' ? 0xFC60 : 0xFFA6;   // red-orange, orange, yellow
            fillRect(dst, x + c, y + r, 1, 1, col);
        }
}

} // namespace

StorageRecord *FurnaceScreen::record()
{
    StorageRecord *r = block_storage.find(BlockPos{fx, fy, fz});
    if(!r || r->kind != StorageKind::FURNACE || r->slots.size() < FURNACE_SLOTS)
        return nullptr;
    return r;
}

void FurnaceScreen::open(int x, int y, int z)
{
    fx = x; fy = y; fz = z;
    // A furnace placed before Survival Edition, or in creative, has no record yet.
    StorageRecord *r = block_storage.find(BlockPos{x, y, z});
    if(!r || r->kind != StorageKind::FURNACE)
        furnacePlaced(x, y, z);
    makeCurrent();
}

void FurnaceScreen::makeCurrent()
{
    if(!background_saved)
        saveBackground();
    held = ItemStack();
    cursor_row = 0;
    cursor_col = 0;
    Task::makeCurrent();
}

void FurnaceScreen::close()
{
    // Whatever is in hand goes back: inventory first, then the ground.
    if(!held.empty())
        returnHeld(player_inventory.slots, PlayerInventory::SLOTS, held);
    if(!held.empty())
        spawnDrop(entity_pool, world_task.x, world_task.y + BLOCK_SIZE, world_task.z, held, player_inventory);
    held = ItemStack();
    world_task.makeCurrent();
}

void FurnaceScreen::tick()
{
    world_task.tickFurnaces();
}

void FurnaceScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    StorageRecord *r = record();
    if(!r)
        return;

    const int px = (SCREEN_WIDTH - PANEL_W) / 2, py = (SCREEN_HEIGHT - PANEL_H) / 2 - 4;
    drawPanel(*screen, px, py, PANEL_W, PANEL_H);
    drawPixelTextCenter(*screen, "Furnace", px + PANEL_W / 2, py + 6, ui::DARK_TEXT, false);
    drawPixelText(*screen, "Inventory", px + 8, py + 73, ui::DARK_TEXT, false);

    for(int i = 0; i < 3; ++i)
    {
        drawSlot(*screen, px + slot_x[i], py + slot_y[i]);
        drawItemStack(*screen, px + slot_x[i] + 1, py + slot_y[i] + 1, r->slots[i]);
    }

    const int lit = r->burn_ticks_left == 0 || r->burn_ticks_total == 0 ? 0
                  : (r->burn_ticks_left * 13 + r->burn_ticks_total - 1) / r->burn_ticks_total + 1;
    drawFlame(*screen, px + FLAME_X, py + FLAME_Y, lit > 14 ? 14 : lit);
    drawProgressArrow(*screen, px + ARROW_X, py + ARROW_Y, r->cook_ticks * 23 / FURNACE_COOK_TICKS);

    const int gx = px + 7;
    drawPlayerSlots(*screen, gx, py + 83);

    int sx, sy;
    const ItemStack *under;
    if(cursor_row == 0)
    {
        sx = px + slot_x[cursor_col];
        sy = py + slot_y[cursor_col];
        under = &r->slots[cursor_col];
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

    drawPixelTextCenter(*screen, "5 take/put   7 half/one   enter send   esc close", SCREEN_WIDTH / 2, py + PANEL_H + 3, ui::TEXT);
}

void FurnaceScreen::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8)
                || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_7)
                || keyPressed(KEY_NSPIRE_ENTER) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN)
                || keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    StorageRecord *r = record();
    if(!r)   // the furnace is gone
    {
        close();
        return;
    }
    ItemStack *top = r->slots.data();

    key_held_down = true;
    const int cols = cursor_row == 0 ? 3 : 9;

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
        close();
    else if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
    {
        const bool up = keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP);
        // Input and fuel are stacked, so 8 / 2 step between them first.
        if(cursor_row == 0 && cursor_col == 0 && !up)
            cursor_col = 1;
        else if(cursor_row == 0 && cursor_col == 1 && up)
            cursor_col = 0;
        else
        {
            const int old_row = cursor_row;
            cursor_row = up ? (cursor_row + 4) % 5 : (cursor_row + 1) % 5;
            if(old_row == 0 && cursor_row != 0)
                cursor_col = slot_x[cursor_col] / SLOT;   // keep roughly the same column
            else if(old_row != 0 && cursor_row == 0)
                cursor_col = cursor_col < 5 ? (up ? 1 : 0) : 2;
        }
    }
    else if(keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_LEFT))
        cursor_col = cursor_row == 0 ? (cursor_col == 2 ? 0 : 2) : (cursor_col + cols - 1) % cols;
    else if(keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_RIGHT))
        cursor_col = cursor_row == 0 ? (cursor_col == 2 ? 0 : 2) : (cursor_col + 1) % cols;
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK) || keyPressed(KEY_NSPIRE_7))
    {
        const bool half = keyPressed(KEY_NSPIRE_7);
        if(cursor_row == 0 && cursor_col == FURNACE_OUTPUT)
        {
            // The output only gives: all of it (or what fits in hand), or half.
            ItemStack &out = top[FURNACE_OUTPUT];
            if(out.empty())
                key_held_down = true;   // nothing to take
            else if(held.empty())
            {
                if(half)
                    moveHalf(top + FURNACE_OUTPUT, 1, 0, held);
                else
                    moveTake(top + FURNACE_OUTPUT, 1, 0, held);
            }
            else if(!half && held.stacksWith(out))
            {
                const unsigned room = itemDef(held.id).max_stack - held.count;
                const unsigned n = out.count < room ? out.count : room;
                held.count = static_cast<uint8_t>(held.count + n);
                out.count = static_cast<uint8_t>(out.count - n);
                if(out.count == 0)
                    out = ItemStack();
            }
        }
        else
        {
            // Only fuel goes in the fuel slot.
            const bool blocked = cursor_row == 0 && cursor_col == FURNACE_FUEL && !held.empty() && fuelTicks(held) == 0;
            ItemStack *slots = cursor_row == 0 ? top : player_inventory.slots;
            const int n = cursor_row == 0 ? 2 : PlayerInventory::SLOTS;
            const int i = cursor_row == 0 ? cursor_col
                                          : (cursor_row < 4 ? 9 + (cursor_row - 1) * 9 + cursor_col : cursor_col);
            if(blocked)
                key_held_down = true;   // refused: not a fuel
            else if(!half)
                moveTake(slots, n, i, held);
            else if(held.empty())
                moveHalf(slots, n, i, held);
            else
                movePlaceOne(slots, n, i, held);
        }
    }
    else if(keyPressed(KEY_NSPIRE_ENTER))
    {
        if(cursor_row == 0)
            moveSendAcross(top, FURNACE_SLOTS, cursor_col, player_inventory.slots, PlayerInventory::SLOTS);
        else
        {
            // Smeltables go in to cook, fuel goes on the fire (1.8.8's shift-click).
            const int i = cursor_row < 4 ? 9 + (cursor_row - 1) * 9 + cursor_col : cursor_col;
            const ItemStack &s = player_inventory.slots[i];
            if(!smeltResult(s).empty())
                moveSendAcross(player_inventory.slots, PlayerInventory::SLOTS, i, top + FURNACE_INPUT, 1);
            else if(fuelTicks(s) > 0)
                moveSendAcross(player_inventory.slots, PlayerInventory::SLOTS, i, top + FURNACE_FUEL, 1);
        }
    }
    else
        key_held_down = false;
}
