#ifndef RECIPELIST_H
#define RECIPELIST_H

#include <cstddef>

#include "crafting.h"
#include "gl.h"

// The scrolling recipe list shared by the inventory screen (simple recipes)
// and the crafting table (all of them). Craftable recipes come first; the
// rest are greyed out.
class RecipeList
{
public:
    // Rebuilds the list, keeping the same recipe selected when it's still there.
    void refresh(const PlayerInventory &inv, bool at_table);
    void reset();
    void move(int delta);

    const Recipe *selected() const;
    bool selectedCraftable(const PlayerInventory &inv) const;

    // Draws a panel with `title` and as many rows as fit in h.
    void draw(TEXTURE &dst, int x, int y, int w, int h, const char *title, bool focused, const PlayerInventory &inv);

    // "3 Planks + 2 Stick"
    static void describe(const Recipe &r, char *buf, size_t size);

    // Where the selected row was last drawn, for a tooltip beside it.
    int sel_x = 0, sel_y = 0;

private:
    int items[MAX_RECIPES];
    int count = 0, sel = 0, scroll = 0, rows = 1;
};

#endif // RECIPELIST_H
