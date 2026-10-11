#include "inventoryscreen.h"

#include <cstdio>

#include "playerinventory.h"
#include "screenmove.h"
#include "uikit.h"
#include "worldtask.h"

InventoryScreen inventory_screen;

constexpr int InventoryScreen::PANEL_W, InventoryScreen::PANEL_H, InventoryScreen::TABLE_PANEL_H, InventoryScreen::LIST_W;

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

void InventoryScreen::open(bool table)
{
    if(!background_saved)
        saveBackground();

    held = ItemStack();
    cursor_row = 3;
    cursor_col = player_inventory.selected;
    at_table = table;
    focus_list = table;          // the table opens on its recipes
    hold_frames = message_frames = 0;
    if(table)
        recipes.reset();
    recipes.refresh(player_inventory, at_table);

    Task::makeCurrent();
}

void InventoryScreen::makeCurrent()
{
    open(false);
}

void InventoryScreen::openCrafting()
{
    open(true);
}

void InventoryScreen::close()
{
    returnHeld(player_inventory.slots, PlayerInventory::SLOTS, held);
    world_task.makeCurrent();
}

void InventoryScreen::craftSelected(bool all)
{
    const Recipe *r = recipes.selected();
    if(!r)
        return;
    if(all)
    {
        if(craftMax(player_inventory, *r) == 0)
        {
            message = hasIngredients(player_inventory, *r) ? "Inventory full" : "Missing ingredients";
            message_frames = 30;
        }
    }
    else switch(craftOnce(player_inventory, *r))
    {
    case CraftResult::OK:
        break;
    case CraftResult::MISSING:
        message = "Missing ingredients";
        message_frames = 30;
        break;
    case CraftResult::NO_ROOM:
        message = "Inventory full";
        message_frames = 30;
        break;
    }
    recipes.refresh(player_inventory, at_table);
}

void InventoryScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    const int panel_h = at_table ? TABLE_PANEL_H : PANEL_H;
    const int lx = (SCREEN_WIDTH - LIST_W - 4 - PANEL_W) / 2, px = lx + LIST_W + 4;
    const int py = (SCREEN_HEIGHT - panel_h) / 2 - (at_table ? 4 : 6);
    recipes.draw(*screen, lx, py, LIST_W, panel_h, "Recipes", focus_list, player_inventory);

    drawPanel(*screen, px, py, PANEL_W, panel_h);
    const int gx = px + 7, gy = py + (at_table ? 83 : 18);
    drawPixelText(*screen, at_table ? "Crafting" : "Inventory", px + 8, py + 6, ui::DARK_TEXT, false);

    const Recipe *r = recipes.selected();
    if(at_table)
    {
        drawPixelText(*screen, "Inventory", px + 8, py + 73, ui::DARK_TEXT, false);

        // The selected recipe: ingredients -> result, with have/need under each.
        static const int slot_x[3] = { 26, 75, 133 };
        constexpr int TOP_Y = 24;
        if(r)
        {
            char buf[16];
            for(int k = 0; k < 2; ++k)
            {
                const Ingredient &in = r->in[k];
                if(in.count == 0)
                    continue;
                const int sx = px + slot_x[k], sy = py + TOP_Y;
                drawSlot(*screen, sx, sy);
                drawItemStack(*screen, sx + 1, sy + 1, ItemStack(in.ids[0], in.count));
                const unsigned have = ingredientHave(player_inventory, in);
                snprintf(buf, sizeof(buf), "%u/%d", have, in.count);
                drawPixelTextCenter(*screen, buf, sx + SLOT / 2, sy + SLOT + 3,
                                    have >= in.count ? ui::DARK_TEXT : static_cast<COLOR>(0xA000), false);
            }
            if(r->in[1].count > 0)
                drawPixelText(*screen, "+", px + 58, py + TOP_Y + 5, ui::DARK_TEXT, false);
            drawPixelText(*screen, "->", px + 104, py + TOP_Y + 5, ui::DARK_TEXT, false);

            const int rx = px + slot_x[2], ry = py + TOP_Y;
            drawSlot(*screen, rx, ry);
            drawItemStack(*screen, rx + 1, ry + 1, ItemStack(r->result, r->result_count));
            if(!hasIngredients(player_inventory, *r))
                darkenRect(*screen, rx + 1, ry + 1, SLOT_INNER, SLOT_INNER);
            drawPixelTextCenter(*screen, itemName(ItemStack(r->result, 1)), px + PANEL_W / 2, py + 58, ui::DARK_TEXT, false);
        }
    }

    drawPlayerSlots(*screen, gx, gy);

    if(focus_list)
    {
        // Without the table's ingredient strip, a tooltip says what the recipe takes.
        if(r && !at_table)
        {
            char what[64];
            RecipeList::describe(*r, what, sizeof(what));
            // Below the row in the upper half of the list, above it lower down.
            const int ty = recipes.sel_y < py + panel_h / 2 ? recipes.sel_y + 15 : recipes.sel_y - 13;
            drawTooltip(*screen, what, recipes.sel_x + 40, ty);
        }
    }
    else
    {
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
    }

    const char *hint = focus_list ? "5 craft   enter craft all   tab slots   esc close"
                                  : "5 take/put   7 half/one   enter send   tab recipes";
    if(message_frames > 0)
    {
        --message_frames;
        drawPixelTextCenter(*screen, message, SCREEN_WIDTH / 2, py + panel_h + 3, ui::YELLOW);
    }
    else
        drawPixelTextCenter(*screen, hint, SCREEN_WIDTH / 2, py + panel_h + 3, ui::TEXT);
}

void InventoryScreen::logic()
{
    if(key_held_down)
    {
        // Holding 5 on a recipe keeps crafting.
        if(focus_list && (keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK)))
        {
            if(++hold_frames > 8 && hold_frames % 2 == 0)
                craftSelected(false);
            return;
        }
        hold_frames = 0;
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8)
                || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_7)
                || keyPressed(KEY_NSPIRE_ENTER) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN)
                || keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_CLICK)
                || keyPressed(KEY_NSPIRE_TAB);
        return;
    }

    key_held_down = true;
    hold_frames = 0;

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
    {
        close();
        return;
    }
    if(keyPressed(KEY_NSPIRE_TAB))
    {
        focus_list = !focus_list;
        return;
    }

    if(focus_list)
    {
        if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
            recipes.move(-1);
        else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
            recipes.move(1);
        else if(keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_RIGHT))
        {
            focus_list = false;
            cursor_col = 0;
        }
        else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
            craftSelected(false);
        else if(keyPressed(KEY_NSPIRE_ENTER))
            craftSelected(true);
        else
            key_held_down = false;
        return;
    }

    ItemStack *slots = player_inventory.slots;
    const int n = PlayerInventory::SLOTS, i = cursorSlot();

    if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
        cursor_row = (cursor_row + 3) % 4;
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
        cursor_row = (cursor_row + 1) % 4;
    else if(keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_LEFT))
    {
        if(cursor_col == 0)
            focus_list = true;
        else
            --cursor_col;
    }
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

    // Moving stacks can change what's craftable.
    recipes.refresh(player_inventory, at_table);
}
