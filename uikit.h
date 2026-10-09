#ifndef UIKIT_H
#define UIKIT_H

#include "gl.h"
#include "items.h"

// Shared drawing for every screen, so the hotbar, inventory, block list,
// menus and later the furnace, chest and trader look the same.
// Everything clips to the destination texture.

constexpr int SLOT = 18, SLOT_INNER = 16;
constexpr int TEXT_LINE = 10;   // line height of the pixel font

namespace ui {
    constexpr COLOR TEXT_WHITE = 0xFFFF, TEXT = 0xE71C, YELLOW = 0xFFF4, GREY_TEXT = 0xA534, DARK_TEXT = 0x4208,
                    TITLE = 0xFE60;
}

void fillRect(TEXTURE &dst, int x, int y, int w, int h, COLOR c);
// Halves the brightness (translucent black) / blends 50% toward white.
void darkenRect(TEXTURE &dst, int x, int y, int w, int h);
void lightenRect(TEXTURE &dst, int x, int y, int w, int h);

// A light grey bevelled panel with a dark outline.
void drawPanel(TEXTURE &dst, int x, int y, int w, int h);
// An 18x18 inset slot; (x, y) is its top-left corner, the item goes at +1, +1.
void drawSlot(TEXTURE &dst, int x, int y);
// The white "hovered" wash over a slot's 16x16 inside, drawn after its item.
void drawSlotHighlight(TEXTURE &dst, int x, int y);
// A wide grey menu button with a centered label.
void drawButton(TEXTURE &dst, int x, int y, int w, int h, const char *label, bool selected, bool enabled = true);
// A dark tooltip box with one line of text, kept on screen.
void drawTooltip(TEXTURE &dst, const char *text, int x, int y);

// The original pixel font, with a one-pixel drop shadow. '\n' starts a new line.
void drawPixelText(TEXTURE &dst, const char *s, int x, int y, COLOR color, bool shadow = true);
void drawPixelTextCenter(TEXTURE &dst, const char *s, int center_x, int y, COLOR color, bool shadow = true);
int pixelTextWidth(const char *s);

// A 16x16 item: block preview or item icon, the count when above 1,
// and a wear bar when a tool is damaged.
void drawItemStack(TEXTURE &dst, int x, int y, const ItemStack &s);
void drawItemIcon(TEXTURE &dst, int x, int y, const ItemStack &s);
// The display name of a stack ("" when empty).
const char *itemName(const ItemStack &s);

// One 9x9 HUD cell from gui_hud (see HudIcon in gui_art.h).
void drawHudIcon(TEXTURE &dst, int icon, int x, int y);

// Copies (part of) a keyed texture with clipping and transparency.
void blitKeyed(const TEXTURE &src, int sx, int sy, int w, int h, TEXTURE &dst, int dx, int dy);

#endif // UIKIT_H
