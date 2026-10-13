#include "inventoryscreen.h"

#include <cstdio>
#include <cstdlib>

#include "drops.h"
#include "entity.h"
#include "playerinventory.h"
#include "screenmove.h"
#include "uikit.h"
#include "worldtask.h"

InventoryScreen inventory_screen;

constexpr int InventoryScreen::PANEL_W, InventoryScreen::PANEL_H, InventoryScreen::LIST_W;

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

namespace {

// Panel-relative layout of the grid, the arrow and the result slot.
struct GridLayout { int x, y, arrow_x, result_x, result_y; };
const GridLayout table_layout = { 30, 17, 90, 124, 35 };
const GridLayout inventory_layout = { 48, 26, 92, 124, 35 };
constexpr int PLAYER_Y = 83;

int playerColFromX(int x)   // panel-relative x to the nearest inventory column
{
    const int c = (x - 7 + SLOT / 2) / SLOT;
    return c < 0 ? 0 : c > 8 ? 8 : c;
}

} // namespace

int InventoryScreen::cursorSlot() const
{
    const int g = gridSize();
    if(cursor_row < g)
        return -1;
    const int pr = cursor_row - g;
    return pr < 3 ? 9 + pr * 9 + cursor_col : cursor_col;
}

void InventoryScreen::open(bool table)
{
    if(!background_saved)
        saveBackground();

    held = ItemStack();
    for(ItemStack &s : grid)
        s = ItemStack();
    at_table = table;
    cursor_row = gridSize() + 3;   // the hotbar
    cursor_col = player_inventory.selected;
    focus_list = false;
    hold_frames = message_frames = 0;
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

void InventoryScreen::emptyGrid()
{
    for(ItemStack &s : grid)
        if(!s.empty())
            returnHeld(player_inventory.slots, PlayerInventory::SLOTS, s);
}

void InventoryScreen::close()
{
    // The grid and the hand go back to the inventory; what doesn't fit drops.
    emptyGrid();
    returnHeld(player_inventory.slots, PlayerInventory::SLOTS, held);
    auto dropRest = [](ItemStack &left) {
        if(!left.empty())
            spawnDrop(entity_pool, world_task.x, world_task.y + BLOCK_SIZE, world_task.z, left, player_inventory);
        left = ItemStack();
    };
    for(ItemStack &s : grid)
        dropRest(s);
    dropRest(held);
    world_task.makeCurrent();
}

void InventoryScreen::showMessage(const char *m)
{
    message = m;
    message_frames = 30;
}

void InventoryScreen::craftSelected(bool all)
{
    const Recipe *r = recipes.selected();
    if(!r)
        return;
    if(all)
    {
        if(craftMax(player_inventory, *r) == 0)
            showMessage(hasIngredients(player_inventory, *r) ? "Inventory full" : "Missing ingredients");
    }
    else switch(craftOnce(player_inventory, *r))
    {
    case CraftResult::OK:
        break;
    case CraftResult::MISSING:
        showMessage("Missing ingredients");
        break;
    case CraftResult::NO_ROOM:
        showMessage("Inventory full");
        break;
    }
    recipes.refresh(player_inventory, at_table);
}

void InventoryScreen::fillFromList()
{
    const Recipe *r = recipes.selected();
    if(!r)
        return;
    const int g = gridSize();
    if(!fillGrid(player_inventory, grid, g, *r))
    {
        // Something else is in the grid: clear it and try once more.
        emptyGrid();
        if(!fillGrid(player_inventory, grid, g, *r))
            showMessage(hasIngredients(player_inventory, *r) ? "No room in the grid" : "Missing ingredients");
    }
    recipes.refresh(player_inventory, at_table);
}

void InventoryScreen::takeResult(bool all)
{
    const int g = gridSize();
    do
    {
        const int i = matchGrid(grid, g);
        if(i < 0)
            break;
        const ItemStack out(recipeAt(i).result, recipeAt(i).result_count);
        if(all)
        {
            // Straight into the inventory, only if all of it fits.
            PlayerInventory after = player_inventory;
            if(after.add(out) != 0)
            {
                showMessage("Inventory full");
                break;
            }
            player_inventory = after;
        }
        else if(held.empty())
            held = out;
        else if(held.stacksWith(out) && held.count + out.count <= itemDef(held.id).max_stack)
            held.count = static_cast<uint8_t>(held.count + out.count);
        else
            break;
        consumeGrid(grid, g);
    } while(all);
    recipes.refresh(player_inventory, at_table);
}

void InventoryScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    const int g = gridSize();
    const GridLayout &L = at_table ? table_layout : inventory_layout;
    const int lx = (SCREEN_WIDTH - LIST_W - 4 - PANEL_W) / 2, px = lx + LIST_W + 4;
    const int py = (SCREEN_HEIGHT - PANEL_H) / 2 - 4;
    recipes.draw(*screen, lx, py, LIST_W, PANEL_H, "Recipes", focus_list, player_inventory);

    drawPanel(*screen, px, py, PANEL_W, PANEL_H);
    drawPixelText(*screen, "Crafting", px + L.x - 2, py + L.y - 11, ui::DARK_TEXT, false);
    drawPixelText(*screen, "Inventory", px + 8, py + 73, ui::DARK_TEXT, false);

    // The grid. Empty and browsing recipes: a faded preview of the picked one.
    bool grid_empty = true;
    for(int i = 0; i < g * g; ++i)
        grid_empty &= grid[i].empty();
    const Recipe *picked = recipes.selected();
    uint8_t ghost[9] = {};
    const bool show_ghost = focus_list && grid_empty && picked && recipeLayout(*picked, g, ghost);

    for(int y = 0; y < g; ++y)
        for(int x = 0; x < g; ++x)
        {
            const int sx = px + L.x + x * SLOT, sy = py + L.y + y * SLOT, i = y * g + x;
            drawSlot(*screen, sx, sy);
            if(!grid[i].empty())
                drawItemStack(*screen, sx + 1, sy + 1, grid[i]);
            else if(show_ghost && ghost[i])
            {
                drawItemIcon(*screen, sx + 1, sy + 1, ItemStack(picked->in[ghost[i] - 1].ids[0], 1));
                darkenRect(*screen, sx + 1, sy + 1, SLOT_INNER, SLOT_INNER);
            }
        }

    const int match = matchGrid(grid, g);
    drawProgressArrow(*screen, px + L.arrow_x, py + L.result_y + 1, match >= 0 ? 22 : 0);
    const int rx = px + L.result_x, ry = py + L.result_y;
    drawSlot(*screen, rx, ry);
    if(match >= 0)
        drawItemStack(*screen, rx + 1, ry + 1, ItemStack(recipeAt(match).result, recipeAt(match).result_count));
    else if(show_ghost)
    {
        drawItemStack(*screen, rx + 1, ry + 1, ItemStack(picked->result, picked->result_count));
        darkenRect(*screen, rx + 1, ry + 1, SLOT_INNER, SLOT_INNER);
    }

    const int gx = px + 7, gy = py + PLAYER_Y;
    drawPlayerSlots(*screen, gx, gy);

    if(focus_list)
    {
        if(picked)
        {
            char what[64];
            RecipeList::describe(*picked, what, sizeof(what));
            // Below the row in the upper half of the list, above it lower down.
            const int ty = recipes.sel_y < py + PANEL_H / 2 ? recipes.sel_y + 15 : recipes.sel_y - 13;
            drawTooltip(*screen, what, recipes.sel_x + 40, ty);
        }
    }
    else
    {
        int sx, sy;
        const ItemStack *under;
        ItemStack result_shown;
        if(cursor_row < g)
        {
            if(cursor_col == g)
            {
                sx = rx; sy = ry;
                if(match >= 0)
                    result_shown = ItemStack(recipeAt(match).result, recipeAt(match).result_count);
                under = &result_shown;
            }
            else
            {
                sx = px + L.x + cursor_col * SLOT; sy = py + L.y + cursor_row * SLOT;
                under = &grid[cursor_row * g + cursor_col];
            }
        }
        else
        {
            playerSlotPos(cursorSlot(), gx, gy, sx, sy);
            under = &player_inventory.slots[cursorSlot()];
        }
        drawSlotHighlight(*screen, sx, sy);
        drawItemStack(*screen, sx + 1, sy + 1, *under);

        if(!held.empty())
            drawItemStack(*screen, sx + 7, sy - 6, held);   // the stack in hand floats over the cursor
        else if(!under->empty())
            drawTooltip(*screen, itemName(*under), sx + 14, sy - 14);
    }

    const char *hint = focus_list ? "5 craft   7 to grid   enter all   tab slots"
                                  : "5 take/put   7 half/one   enter send   tab recipes";
    if(message_frames > 0)
    {
        --message_frames;
        drawPixelTextCenter(*screen, message, SCREEN_WIDTH / 2, py + PANEL_H + 3, ui::YELLOW);
    }
    else
        drawPixelTextCenter(*screen, hint, SCREEN_WIDTH / 2, py + PANEL_H + 3, ui::TEXT);
}

void InventoryScreen::moveCursorVertical(bool up)
{
    const int g = gridSize(), rows = g + 4;
    const GridLayout &L = at_table ? table_layout : inventory_layout;
    const bool in_grid = cursor_row < g;

    // The result slot sits beside the grid: 8 / 2 leave it for the inventory.
    if(in_grid && cursor_col == g)
    {
        cursor_row = up ? g + 3 : g;
        cursor_col = playerColFromX(L.result_x);
        return;
    }

    const int next = up ? (cursor_row + rows - 1) % rows : (cursor_row + 1) % rows;
    const bool next_in_grid = next < g;
    if(in_grid && !next_in_grid)
        cursor_col = playerColFromX(L.x + cursor_col * SLOT);
    else if(!in_grid && next_in_grid)
    {
        // The nearest grid column, or the result slot if that's closer.
        const int x = 7 + cursor_col * SLOT;
        int best = 0;
        for(int c = 1; c <= g; ++c)
        {
            const int cx = c == g ? L.result_x : L.x + c * SLOT;
            const int bx = best == g ? L.result_x : L.x + best * SLOT;
            if(abs(cx - x) < abs(bx - x))
                best = c;
        }
        cursor_col = best;
    }
    cursor_row = next;
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
            cursor_row = 0;
            cursor_col = 0;   // the grid's top left
        }
        else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
            craftSelected(false);
        else if(keyPressed(KEY_NSPIRE_7))
            fillFromList();
        else if(keyPressed(KEY_NSPIRE_ENTER))
            craftSelected(true);
        else
            key_held_down = false;
        return;
    }

    const int g = gridSize();
    const bool in_grid = cursor_row < g, on_result = in_grid && cursor_col == g;
    const int cols = in_grid ? g + 1 : 9;

    if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
        moveCursorVertical(true);
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
        moveCursorVertical(false);
    else if(keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_LEFT))
    {
        if(cursor_col == 0)
            focus_list = true;
        else
            --cursor_col;
    }
    else if(keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_RIGHT))
        cursor_col = (cursor_col + 1) % cols;
    else if(on_result)
    {
        if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK) || keyPressed(KEY_NSPIRE_7))
            takeResult(false);
        else if(keyPressed(KEY_NSPIRE_ENTER))
            takeResult(true);
        else
            key_held_down = false;
    }
    else
    {
        ItemStack *slots = in_grid ? grid : player_inventory.slots;
        const int n = in_grid ? g * g : PlayerInventory::SLOTS;
        const int i = in_grid ? cursor_row * g + cursor_col : cursorSlot();

        if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
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
            ItemStack *inv = player_inventory.slots;
            if(in_grid)
                moveSendAcross(grid, n, i, inv, PlayerInventory::SLOTS);
            else if(i < PlayerInventory::HOTBAR)
                moveSendAcross(inv, PlayerInventory::HOTBAR, i, inv + PlayerInventory::HOTBAR, n - PlayerInventory::HOTBAR);
            else
                moveSendAcross(inv + PlayerInventory::HOTBAR, n - PlayerInventory::HOTBAR, i - PlayerInventory::HOTBAR, inv, PlayerInventory::HOTBAR);
        }
        else
            key_held_down = false;
    }

    // Moving stacks can change what's craftable.
    recipes.refresh(player_inventory, at_table);
}
