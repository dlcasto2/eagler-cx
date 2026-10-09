#include "uikit.h"

#include <cstdio>

#include "blockrenderer.h"
#include "gui_art.h"
#include "item_icons.h"
#include "texturetools.h"

namespace {

constexpr COLOR KEY = 0xF81F;

inline bool inside(const TEXTURE &t, int x, int y)
{
    return x >= 0 && y >= 0 && x < t.width && y < t.height;
}

inline void put(TEXTURE &t, int x, int y, COLOR c)
{
    if(inside(t, x, y))
        t.bitmap[x + y * t.width] = c;
}

// Darkens a colour to a quarter, for text shadows.
inline COLOR shadowOf(COLOR c)
{
    return (c >> 2) & 0x39E7;
}

// A small deterministic hash, used to speckle the buttons like stone.
inline unsigned speckle(int x, int y)
{
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) & 7;
}

inline COLOR grey(unsigned v)
{
    return static_cast<COLOR>(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3));
}

// ---- block previews, shrunk from Crafti's 24-pixel previews to 16 and cached

struct PreviewEntry {
    BLOCK_WDATA block;
    bool used;
    COLOR px[SLOT_INNER * SLOT_INNER];
};

constexpr int PREVIEW_CACHE = 64; // the block list shows 44 blocks plus the hotbar
PreviewEntry preview_cache[PREVIEW_CACHE];
int preview_next = 0;

const COLOR *blockPreview16(BLOCK_WDATA block)
{
    for(auto &e : preview_cache)
        if(e.used && e.block == block)
            return e.px;

    static COLOR scratch_px[24 * 32];
    TEXTURE scratch = { 24, 32, true, KEY, scratch_px };
    for(auto &p : scratch_px)
        p = KEY;
    global_block_renderer.drawPreview(block, scratch, 0, 0);

    PreviewEntry &e = preview_cache[preview_next];
    preview_next = (preview_next + 1) % PREVIEW_CACHE;
    e.block = block;
    e.used = true;

    // Doors are 32 tall: shrink by half. Everything else by two thirds.
    const int src_h = getBLOCK(block) == BLOCK_DOOR ? 32 : 24;
    const int dst_w = 24 * SLOT_INNER / src_h, off_x = (SLOT_INNER - dst_w) / 2;
    for(int y = 0; y < SLOT_INNER; ++y)
        for(int x = 0; x < SLOT_INNER; ++x)
        {
            COLOR out = KEY;
            const int dx = x - off_x;
            if(dx >= 0 && dx < dst_w)
            {
                // Average the opaque pixels this output pixel covers.
                const int x0 = dx * src_h / SLOT_INNER, x1 = (dx + 1) * src_h / SLOT_INNER;
                const int y0 = y * src_h / SLOT_INNER, y1 = (y + 1) * src_h / SLOT_INNER;
                unsigned r = 0, g = 0, b = 0, n = 0, total = 0;
                for(int sy = y0; sy < y1; ++sy)
                    for(int sx = x0; sx < x1; ++sx)
                    {
                        ++total;
                        const COLOR c = scratch_px[sx + sy * 24];
                        if(c == KEY)
                            continue;
                        r += c >> 11; g += (c >> 5) & 63; b += c & 31; ++n;
                    }
                if(n * 2 >= total && n > 0)
                {
                    out = static_cast<COLOR>(((r / n) << 11) | ((g / n) << 5) | (b / n));
                    if(out == KEY)
                        out ^= 0x20;
                }
            }
            e.px[x + y * SLOT_INNER] = out;
        }
    return e.px;
}

} // namespace

void blitKeyed(const TEXTURE &src, int sx, int sy, int w, int h, TEXTURE &dst, int dx, int dy)
{
    for(int y = 0; y < h; ++y)
        for(int x = 0; x < w; ++x)
        {
            const COLOR c = src.bitmap[sx + x + (sy + y) * src.width];
            if(src.has_transparency && c == src.transparent_color)
                continue;
            put(dst, dx + x, dy + y, c);
        }
}

void fillRect(TEXTURE &dst, int x, int y, int w, int h, COLOR c)
{
    for(int yy = y; yy < y + h; ++yy)
        for(int xx = x; xx < x + w; ++xx)
            put(dst, xx, yy, c);
}

void darkenRect(TEXTURE &dst, int x, int y, int w, int h)
{
    for(int yy = y; yy < y + h; ++yy)
        for(int xx = x; xx < x + w; ++xx)
            if(inside(dst, xx, yy))
            {
                COLOR &p = dst.bitmap[xx + yy * dst.width];
                p = (p >> 1) & 0x7BEF;
            }
}

void lightenRect(TEXTURE &dst, int x, int y, int w, int h)
{
    for(int yy = y; yy < y + h; ++yy)
        for(int xx = x; xx < x + w; ++xx)
            if(inside(dst, xx, yy))
            {
                COLOR &p = dst.bitmap[xx + yy * dst.width];
                p = ((p >> 1) & 0x7BEF) + 0x7BEF;
            }
}

void drawPanel(TEXTURE &dst, int x, int y, int w, int h)
{
    const COLOR outline = 0x0000, light = 0xFFFF, dark = grey(0x55), fill = grey(0xC6);
    fillRect(dst, x + 1, y + 1, w - 2, h - 2, fill);
    // outline with the corners cut off, so the panel looks rounded
    fillRect(dst, x + 2, y, w - 4, 1, outline);
    fillRect(dst, x + 2, y + h - 1, w - 4, 1, outline);
    fillRect(dst, x, y + 2, 1, h - 4, outline);
    fillRect(dst, x + w - 1, y + 2, 1, h - 4, outline);
    put(dst, x + 1, y + 1, outline);
    put(dst, x + w - 2, y + 1, outline);
    put(dst, x + 1, y + h - 2, outline);
    put(dst, x + w - 2, y + h - 2, outline);
    // bevel: light top-left, dark bottom-right
    fillRect(dst, x + 2, y + 1, w - 4, 2, light);
    fillRect(dst, x + 1, y + 2, 2, h - 5, light);
    fillRect(dst, x + 3, y + h - 3, w - 5, 2, dark);
    fillRect(dst, x + w - 3, y + 3, 2, h - 5, dark);
    put(dst, x + 2, y + 2, light);
    put(dst, x + w - 3, y + h - 3, dark);
}

void drawSlot(TEXTURE &dst, int x, int y)
{
    fillRect(dst, x, y, SLOT - 1, 1, grey(0x37));
    fillRect(dst, x, y, 1, SLOT - 1, grey(0x37));
    fillRect(dst, x + 1, y + SLOT - 1, SLOT - 1, 1, 0xFFFF);
    fillRect(dst, x + SLOT - 1, y + 1, 1, SLOT - 1, 0xFFFF);
    put(dst, x + SLOT - 1, y, grey(0x8B));
    put(dst, x, y + SLOT - 1, grey(0x8B));
    fillRect(dst, x + 1, y + 1, SLOT_INNER, SLOT_INNER, grey(0x8B));
}

void drawSlotHighlight(TEXTURE &dst, int x, int y)
{
    lightenRect(dst, x + 1, y + 1, SLOT_INNER, SLOT_INNER);
}

void drawButton(TEXTURE &dst, int x, int y, int w, int h, const char *label, bool selected, bool enabled)
{
    // stone-grey face with a speckle; selected buttons turn blue-violet
    for(int yy = 1; yy < h - 1; ++yy)
        for(int xx = 1; xx < w - 1; ++xx)
        {
            const unsigned s = speckle(xx, yy);
            COLOR c;
            if(!enabled)
                c = grey(0x2C + s);
            else if(selected)
                c = static_cast<COLOR>((((0x6A + 2 * s) >> 3) << 11) | (((0x72 + 2 * s) >> 2) << 5) | ((0xB8 + 2 * s) >> 3));
            else
                c = grey(0x66 + 3 * s);
            put(dst, x + xx, y + yy, c);
        }
    const COLOR top = !enabled ? grey(0x40) : selected ? 0xC69F : grey(0xA8);
    const COLOR bottom = !enabled ? grey(0x20) : selected ? 0x52B4 : grey(0x4A);
    fillRect(dst, x + 1, y + 1, w - 2, 1, top);
    fillRect(dst, x + 1, y + 1, 1, h - 3, top);
    fillRect(dst, x + 1, y + h - 2, w - 2, 1, bottom);
    fillRect(dst, x + w - 2, y + 2, 1, h - 3, bottom);
    // outline: black normally, white when selected
    const COLOR edge = selected && enabled ? 0xFFFF : 0x0000;
    fillRect(dst, x, y, w, 1, edge);
    fillRect(dst, x, y + h - 1, w, 1, edge);
    fillRect(dst, x, y, 1, h, edge);
    fillRect(dst, x + w - 1, y, 1, h, edge);

    const COLOR text = !enabled ? grey(0xA0) : selected ? ui::YELLOW : ui::TEXT;
    drawPixelTextCenter(dst, label, x + w / 2, y + (h - 7) / 2, text);
}

void drawTooltip(TEXTURE &dst, const char *text, int x, int y)
{
    const int w = pixelTextWidth(text) + 6, h = 13;
    if(x + w > dst.width - 1) x = dst.width - 1 - w;
    if(x < 1) x = 1;
    if(y + h > dst.height - 1) y = dst.height - 1 - h;
    if(y < 1) y = 1;
    fillRect(dst, x + 1, y, w - 2, h, 0x1002);
    fillRect(dst, x, y + 1, w, h - 2, 0x1002);
    // a violet inner border
    fillRect(dst, x + 1, y + 1, w - 2, 1, 0x500F);
    fillRect(dst, x + 1, y + h - 2, w - 2, 1, 0x2808);
    fillRect(dst, x + 1, y + 1, 1, h - 2, 0x400C);
    fillRect(dst, x + w - 2, y + 1, 1, h - 2, 0x400C);
    drawPixelText(dst, text, x + 3, y + 3, 0xFFFF);
}

static int drawGlyphs(TEXTURE &dst, const char *s, int x, int y, COLOR color)
{
    const int start_x = x;
    int max_x = x;
    for(; *s; ++s)
    {
        const unsigned char ch = static_cast<unsigned char>(*s);
        if(ch == '\n')
        {
            x = start_x;
            y += TEXT_LINE;
            continue;
        }
        if(ch < 32 || ch > 126)
            continue;
        const int i = ch - 32, w = gui_font_widths[i];
        for(int row = 0; row < 9; ++row)
        {
            const uint8_t bits = gui_font_rows[i][row];
            if(!bits)
                continue;
            for(int col = 0; col < w; ++col)
                if(bits & (1 << (4 - col)))
                    put(dst, x + col, y + row, color);
        }
        x += w + 1;
        if(x > max_x)
            max_x = x;
    }
    return max_x - start_x;
}

void drawPixelText(TEXTURE &dst, const char *s, int x, int y, COLOR color, bool shadow)
{
    if(shadow)
        drawGlyphs(dst, s, x + 1, y + 1, shadowOf(color));
    drawGlyphs(dst, s, x, y, color);
}

int pixelTextWidth(const char *s)
{
    int w = 0, best = 0;
    for(; *s; ++s)
    {
        const unsigned char ch = static_cast<unsigned char>(*s);
        if(ch == '\n')
        {
            w = 0;
            continue;
        }
        if(ch < 32 || ch > 126)
            continue;
        w += gui_font_widths[ch - 32] + 1;
        if(w > best)
            best = w;
    }
    return best > 0 ? best - 1 : 0;
}

void drawPixelTextCenter(TEXTURE &dst, const char *s, int center_x, int y, COLOR color, bool shadow)
{
    drawPixelText(dst, s, center_x - pixelTextWidth(s) / 2, y, color, shadow);
}

void drawItemIcon(TEXTURE &dst, int x, int y, const ItemStack &s)
{
    if(s.empty())
        return;

    if(isBlockItem(s.id))
    {
        const COLOR *px = blockPreview16(s.toBlock());
        for(int yy = 0; yy < SLOT_INNER; ++yy)
            for(int xx = 0; xx < SLOT_INNER; ++xx)
            {
                const COLOR c = px[xx + yy * SLOT_INNER];
                if(c != KEY)
                    put(dst, x + xx, y + yy, c);
            }
        return;
    }

    int u, v;
    if(itemIconUV(s.id, u, v))
        blitKeyed(itemIconAtlas(), u, v, 16, 16, dst, x, y);
}

void drawItemStack(TEXTURE &dst, int x, int y, const ItemStack &s)
{
    if(s.empty())
        return;

    drawItemIcon(dst, x, y, s);

    const ItemDef &def = itemDef(s.id);
    if(def.durability > 0 && s.meta > 0)
    {
        // wear bar: green when new, through yellow to red when nearly broken
        const int left = def.durability > s.meta ? def.durability - s.meta : 0;
        const int len = (13 * left + def.durability / 2) / def.durability;
        const int red = 31 - 31 * left / def.durability, green = 63 * left / def.durability;
        fillRect(dst, x + 2, y + 13, 13, 2, 0x0000);
        fillRect(dst, x + 2, y + 13, len, 1, static_cast<COLOR>((red << 11) | (green << 5)));
    }

    if(s.count > 1)
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(s.count));
        drawPixelText(dst, buf, x + 16 - pixelTextWidth(buf), y + 8, 0xFFFF);
    }
}

const char *itemName(const ItemStack &s)
{
    if(s.empty())
        return "";
    if(isBlockItem(s.id))
        return global_block_renderer.getName(s.toBlock());
    const char *n = itemDef(s.id).name;
    return n ? n : "";
}

void drawHudIcon(TEXTURE &dst, int icon, int x, int y)
{
    blitKeyed(gui_hud, icon * 9, 0, 9, 9, dst, x, y);
}
