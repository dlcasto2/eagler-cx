#include "mono_art.h"

#include <libndls.h>

#include "texturetools.h"
#include "gui_art.h"
#include "item_icons.h"

namespace {

// Like nGL's greyscaleTexture(), but leaves pixels equal to the texture's
// transparent_color untouched. gui_hud, gui_title and the item icon atlas are
// composited with blitKeyed(), which matches that exact key against the live
// bitmap on every draw; nGL's own greyscaleTexture() would remap every pixel
// (its transparent key 0xF81F converts to a small value like any other
// colour), so the key would stop matching and every icon would draw as a
// solid block instead of a cut-out.
void greyscaleKeepingKey(TEXTURE &tex)
{
    unsigned int pixels = tex.width * tex.height;
    COLOR *ptr = tex.bitmap;
    while(pixels--)
    {
        if(*ptr != tex.transparent_color)
        {
            const RGB rgb = rgbColor(*ptr);
            *ptr = static_cast<COLOR>((rgb.r.value + rgb.g.value + rgb.g.value + rgb.b.value) >> 5);
        }
        ++ptr;
    }
}

} // namespace

void monoArtInit()
{
    if(lcd_type() != SCR_320x240_4)
        return;

    // gui_hud, gui_title and the item icon atlas are blitKeyed() against
    // 0xF81F, which greyscaling must preserve.
    greyscaleKeepingKey(gui_hud);
    greyscaleKeepingKey(gui_title);
    // itemIconAtlas() only exposes a const reference, but the TEXTURE behind
    // it is never actually const, so converting it in place is well-defined.
    greyscaleKeepingKey(const_cast<TEXTURE &>(itemIconAtlas()));

    // gui_crack/gui_sun/gui_moon are drawn through the 3D path instead (bound
    // with glBindTexture), whose transparency rule hardcodes literal black
    // rather than reading transparent_color; black happens to convert to
    // itself under the same formula, so plain greyscaleTexture() is safe here.
    greyscaleTexture(gui_crack);
    greyscaleTexture(gui_sun);
    greyscaleTexture(gui_moon);
}
