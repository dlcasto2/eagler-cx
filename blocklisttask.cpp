#include "blocklisttask.h"

#include <algorithm>

#include "blockrenderer.h"
#include "playerinventory.h"
#include "uikit.h"
#include "inventory.h"
#include "terrain.h"
#include "texturetools.h"
#include "worldtask.h"

BlockListTask block_list_task;

static const BLOCK_WDATA user_selectable[] = {
    BLOCK_STONE,
    BLOCK_COBBLESTONE,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_PLANKS_NORMAL,
    BLOCK_PLANKS_DARK,
    BLOCK_PLANKS_BRIGHT,
    BLOCK_WALL,
    BLOCK_GLASS,
    BLOCK_DOOR,
    BLOCK_COAL_ORE,
    BLOCK_GOLD_ORE,
    BLOCK_IRON_ORE,
    BLOCK_DIAMOND_ORE,
    BLOCK_REDSTONE_ORE,
    BLOCK_EMERALD_ORE,
    BLOCK_ANCIENT_DEBRIS,
    BLOCK_IRON,
    BLOCK_GOLD,
    BLOCK_DIAMOND,
    BLOCK_GLOWSTONE,
    BLOCK_NETHERRACK,
    BLOCK_TNT,
    BLOCK_SPONGE,
    BLOCK_FURNACE,
    BLOCK_CRAFTING_TABLE,
    BLOCK_BOOKSHELF,
    BLOCK_PUMPKIN,
    getBLOCKWDATA(BLOCK_WATER, RANGE_WATER),
    getBLOCKWDATA(BLOCK_LAVA, RANGE_LAVA),
    getBLOCKWDATA(BLOCK_FLOWER, 0),
    getBLOCKWDATA(BLOCK_FLOWER, 1),
    getBLOCKWDATA(BLOCK_MUSHROOM, 0),
    getBLOCKWDATA(BLOCK_MUSHROOM, 1),
    getBLOCKWDATA(BLOCK_WHEAT, 0),
    BLOCK_SPIDERWEB,
    BLOCK_TORCH,
    BLOCK_CAKE,
    BLOCK_REDSTONE_LAMP,
    BLOCK_REDSTONE_SWITCH,
    BLOCK_PRESSURE_PLATE,
    BLOCK_REDSTONE_WIRE,
    BLOCK_REDSTONE_TORCH
};

constexpr int user_selectable_count = sizeof(user_selectable)/sizeof(*user_selectable);

BlockListTask::BlockListTask()
{
    static_assert(fields_x * fields_y >= sizeof(user_selectable)/sizeof(*user_selectable), "Not enough fields");
}

void BlockListTask::makeCurrent()
{
    if(!background_saved)
        saveBackground();

    Task::makeCurrent();
}

void BlockListTask::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // A creative-style panel: the block grid on top, the hotbar row below.
    const int px = (SCREEN_WIDTH - panel_width) / 2, py = (SCREEN_HEIGHT - panel_height) / 2 - 6;
    drawPanel(*screen, px, py, panel_width, panel_height);
    drawPixelText(*screen, "Blocks", px + 8, py + 6, ui::DARK_TEXT, false);

    const int gx = px + 7, gy = py + 18;
    for(int i = 0; i < fields_x * fields_y; ++i)
    {
        const int sx = gx + SLOT * (i % fields_x), sy = gy + SLOT * (i / fields_x);
        drawSlot(*screen, sx, sy);
        if(i < user_selectable_count)
            drawItemStack(*screen, sx + 1, sy + 1, ItemStack::ofBlock(user_selectable[i]));
    }

    const int hy = gy + SLOT * fields_y + 4;
    for(int i = 0; i < PlayerInventory::HOTBAR; ++i)
    {
        drawSlot(*screen, gx + SLOT * i, hy);
        drawItemStack(*screen, gx + SLOT * i + 1, hy + 1, player_inventory.slots[i]);
    }
    // the slot that 5 will fill
    const int hx = gx + SLOT * player_inventory.selected;
    fillRect(*screen, hx, hy - 1, SLOT, 1, 0xFFFF);
    fillRect(*screen, hx, hy + SLOT, SLOT, 1, 0xFFFF);
    fillRect(*screen, hx - 1, hy - 1, 1, SLOT + 2, 0xFFFF);
    fillRect(*screen, hx + SLOT, hy - 1, 1, SLOT + 2, 0xFFFF);

    const int cx = gx + SLOT * (current_selection % fields_x), cy = gy + SLOT * (current_selection / fields_x);
    drawSlotHighlight(*screen, cx, cy);
    drawItemStack(*screen, cx + 1, cy + 1, ItemStack::ofBlock(user_selectable[current_selection]));
    drawTooltip(*screen, global_block_renderer.getName(user_selectable[current_selection]), cx + 14, cy - 14);

    drawPixelTextCenter(*screen, "5 put in slot   1/3 pick slot   . close", SCREEN_WIDTH / 2, py + panel_height + 4, ui::TEXT);
}

void BlockListTask::logic()
{
    if(key_held_down)
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_1) || keyPressed(KEY_NSPIRE_3) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN) || keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_RIGHT)  || keyPressed(KEY_NSPIRE_CLICK);
    else if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
    {
        world_task.makeCurrent();

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
    {
        current_selection += fields_x;
        if(current_selection >= user_selectable_count)
            current_selection %= fields_x;

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
    {
        if(current_selection >= fields_x)
            current_selection -= fields_x;
        else
        {
            current_selection = ((user_selectable_count - 1) / fields_x) * fields_x + (current_selection % fields_x);
            if(current_selection >= user_selectable_count)
                current_selection -= fields_x;
        }

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_LEFT))
    {
        if(current_selection % fields_x == 0)
        {
            current_selection += fields_x - 1;
            if(current_selection >= user_selectable_count)
                current_selection = user_selectable_count - 1;
        }
        else
            current_selection--;

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_RIGHT))
    {
        if(current_selection % fields_x != fields_x-1 && current_selection < user_selectable_count - 1)
            current_selection++;
        else
            current_selection -= current_selection % fields_x;

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_1)) //Switch inventory slot
    {
        current_inventory.previousSlot();

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_3))
    {
        current_inventory.nextSlot();

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
    {
        current_inventory.setCurrentBlock(user_selectable[current_selection]);

        key_held_down = true;
    }
}
