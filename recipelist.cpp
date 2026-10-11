#include "recipelist.h"

#include <cstdio>
#include <cstring>

#include "uikit.h"

namespace {

constexpr COLOR grey565(unsigned v) { return static_cast<COLOR>(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3)); }

constexpr int ROW_H = 18, TOP = 17;

// Copies s into buf, cut short with '.' so it fits in max_w pixels.
const char *fitText(const char *s, char *buf, size_t size, int max_w)
{
    snprintf(buf, size, "%s", s);
    size_t len = strlen(buf);
    if(pixelTextWidth(buf) <= max_w)
        return buf;
    while(len > 1)
    {
        buf[--len] = 0;
        buf[len - 1] = '.';
        if(pixelTextWidth(buf) <= max_w)
            break;
    }
    return buf;
}

} // namespace

void RecipeList::refresh(const PlayerInventory &inv, bool at_table)
{
    const int keep = count > 0 ? items[sel] : -1;
    count = listRecipes(inv, at_table, items, MAX_RECIPES);
    if(sel >= count)
        sel = count > 0 ? count - 1 : 0;
    for(int i = 0; i < count; ++i)
        if(items[i] == keep)
            sel = i;
    move(0);
}

void RecipeList::reset()
{
    sel = scroll = 0;
}

void RecipeList::move(int delta)
{
    if(count == 0)
        return;
    sel = (sel + delta + count) % count;
    if(sel < scroll)
        scroll = sel;
    if(sel >= scroll + rows)
        scroll = sel - rows + 1;
    // Never leave empty rows below the last recipe.
    const int max_scroll = count > rows ? count - rows : 0;
    if(scroll > max_scroll)
        scroll = max_scroll;
}

const Recipe *RecipeList::selected() const
{
    return count > 0 ? &recipeAt(items[sel]) : nullptr;
}

bool RecipeList::selectedCraftable(const PlayerInventory &inv) const
{
    const Recipe *r = selected();
    return r && hasIngredients(inv, *r);
}

void RecipeList::describe(const Recipe &r, char *buf, size_t size)
{
    if(r.in[1].count > 0)
        snprintf(buf, size, "%d %s + %d %s", r.in[0].count, ingredientName(r.in[0]), r.in[1].count, ingredientName(r.in[1]));
    else
        snprintf(buf, size, "%d %s", r.in[0].count, ingredientName(r.in[0]));
}

void RecipeList::draw(TEXTURE &dst, int x, int y, int w, int h, const char *title, bool focused, const PlayerInventory &inv)
{
    drawPanel(dst, x, y, w, h);
    drawPixelText(dst, title, x + 8, y + 6, ui::DARK_TEXT, false);

    // An inset well holds the rows.
    const int wx = x + 6, wy = y + TOP, ww = w - 12;
    rows = (h - TOP - 6) / ROW_H;
    if(rows < 1)
        rows = 1;
    move(0);
    const int wh = rows * ROW_H;
    fillRect(dst, wx - 1, wy - 1, ww + 2, 1, grey565(0x37));
    fillRect(dst, wx - 1, wy - 1, 1, wh + 2, grey565(0x37));
    fillRect(dst, wx, wy + wh, ww + 1, 1, 0xFFFF);
    fillRect(dst, wx + ww, wy, 1, wh + 1, 0xFFFF);
    fillRect(dst, wx, wy, ww, wh, grey565(0x8B));

    const bool scrollbar = count > rows;
    const int text_w = ww - 21 - (scrollbar ? 4 : 0);
    char name[40];
    for(int r = 0; r < rows && scroll + r < count; ++r)
    {
        const int i = scroll + r, ry = wy + r * ROW_H;
        const Recipe &rec = recipeAt(items[i]);
        const bool ok = hasIngredients(inv, rec);

        if(i == sel)
        {
            if(focused)
                lightenRect(dst, wx, ry, ww - (scrollbar ? 4 : 0), ROW_H);
            else
                fillRect(dst, wx, ry, 2, ROW_H, 0xFFFF);
            sel_x = wx;
            sel_y = ry;
        }

        const ItemStack out(rec.result, rec.result_count);
        drawItemStack(dst, wx + 1, ry + 1, out);
        if(!ok)
            darkenRect(dst, wx + 1, ry + 1, SLOT_INNER, SLOT_INNER);
        fitText(itemName(out), name, sizeof(name), text_w);
        if(ok)
            drawPixelText(dst, name, wx + 20, ry + 5, ui::TEXT_WHITE);
        else
            drawPixelText(dst, name, wx + 20, ry + 5, grey565(0x50), false);
    }

    if(count == 0)
        drawPixelText(dst, "Nothing", wx + 4, wy + 5, ui::DARK_TEXT, false);

    if(scrollbar)
    {
        const int bar_h = wh * rows / count < 6 ? 6 : wh * rows / count;
        const int bar_y = wy + (wh - bar_h) * scroll / (count - rows);
        fillRect(dst, wx + ww - 3, wy, 3, wh, grey565(0x55));
        fillRect(dst, wx + ww - 3, bar_y, 3, bar_h, 0xFFFF);
    }
}
