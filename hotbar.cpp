#include "hotbar.h"

#include "gamestate.h"
#include "gui_art.h"
#include "playerinventory.h"
#include "uikit.h"

void drawHotbar(TEXTURE &dst)
{
    const int x = HOTBAR_X, y = HOTBAR_Y;

    // translucent dark bar with a grey frame and dark slot wells
    darkenRect(dst, x + 1, y + 1, HOTBAR_W - 2, HOTBAR_H - 2);
    fillRect(dst, x, y, HOTBAR_W, 1, 0x0000);
    fillRect(dst, x, y + HOTBAR_H - 1, HOTBAR_W, 1, 0x0000);
    fillRect(dst, x, y, 1, HOTBAR_H, 0x0000);
    fillRect(dst, x + HOTBAR_W - 1, y, 1, HOTBAR_H, 0x0000);
    fillRect(dst, x + 1, y + 1, HOTBAR_W - 2, 1, 0x8C51);
    fillRect(dst, x + 1, y + HOTBAR_H - 2, HOTBAR_W - 2, 1, 0x4208);
    for(int i = 0; i < PlayerInventory::HOTBAR; ++i)
    {
        const int sx = x + 1 + 20 * i;
        darkenRect(dst, sx + 2, y + 3, 16, 16);
        if(i > 0)
            fillRect(dst, sx, y + 2, 1, HOTBAR_H - 4, 0x6B4D);
    }

    for(int i = 0; i < PlayerInventory::HOTBAR; ++i)
        drawItemStack(dst, x + 3 + 20 * i, y + 3, player_inventory.slots[i]);

    // selection: a bright 24x24 frame around the chosen slot
    const int sx = x - 1 + 20 * player_inventory.selected, sy = y - 1;
    fillRect(dst, sx, sy, 24, 1, 0x0000);
    fillRect(dst, sx, sy + 23, 24, 1, 0x0000);
    fillRect(dst, sx, sy, 1, 24, 0x0000);
    fillRect(dst, sx + 23, sy, 1, 24, 0x0000);
    fillRect(dst, sx + 1, sy + 1, 22, 2, 0xFFFF);
    fillRect(dst, sx + 1, sy + 21, 22, 2, 0xC618);
    fillRect(dst, sx + 1, sy + 1, 2, 22, 0xFFFF);
    fillRect(dst, sx + 21, sy + 1, 2, 22, 0xC618);
}

int drawStatusBars(TEXTURE &dst, bool underwater)
{
    if(player_state.mode != GameMode::SURVIVAL)
        return HOTBAR_Y - 2;

    const int y = HOTBAR_Y - 11;
    for(int i = 0; i < 10; ++i)
    {
        const int hp = player_state.health - 2 * i, food = player_state.hunger - 2 * i;
        drawHudIcon(dst, hp >= 2 ? HUD_HEART : hp == 1 ? HUD_HEART_HALF : HUD_HEART_EMPTY, HOTBAR_X + 8 * i, y);
        // hunger fills from the right, as in 1.8.8
        drawHudIcon(dst, food >= 2 ? HUD_FOOD : food == 1 ? HUD_FOOD_HALF : HUD_FOOD_EMPTY, HOTBAR_X + HOTBAR_W - 9 - 8 * i, y);
    }

    if(!underwater || player_state.air_ticks >= 300)
        return y - 2;

    // ten bubbles, one per 30 ticks of air; the next one to go pops
    const int air = player_state.air_ticks, full = (air + 29) / 30;
    const int by = y - 10;
    for(int i = 0; i < full; ++i)
        drawHudIcon(dst, (i == full - 1 && air % 30 != 0 && air % 30 < 6) ? HUD_BUBBLE_POP : HUD_BUBBLE,
                    HOTBAR_X + HOTBAR_W - 9 - 8 * i, by);
    return by - 2;
}

void hotbarPrev()
{
    if(--player_inventory.selected < 0)
        player_inventory.selected = PlayerInventory::HOTBAR - 1;
}

void hotbarNext()
{
    if(++player_inventory.selected >= PlayerInventory::HOTBAR)
        player_inventory.selected = 0;
}
