#include "block_art.h"


namespace {

// Atlas cells in row 4 whose stock textures (spawner, snow) Crafti doesn't use.
constexpr int DEBRIS_SIDE_CELL = 1, DEBRIS_TOP_CELL = 2, ART_ROW = 4;

// Nearest-neighbour scale of a 16x16 piece of art into one atlas cell,
// skipping its transparent pixels.
void paint(const TEXTURE &art, TEXTURE &terrain, int cell_x, int cell_y, int fw, int fh)
{
    for(int y = 0; y < fh; ++y)
        for(int x = 0; x < fw; ++x)
        {
            const COLOR c = art.bitmap[(x * art.width / fw) + (y * art.height / fh) * art.width];
            if(art.has_transparency && c == art.transparent_color)
                continue;
            terrain.bitmap[cell_x * fw + x + (cell_y * fh + y) * terrain.width] = c;
        }
}

} // namespace

void paintAddedBlockTextures(TEXTURE &terrain, int fw, int fh)
{
    paint(art_debris_side, terrain, DEBRIS_SIDE_CELL, ART_ROW, fw, fh);
    paint(art_debris_top, terrain, DEBRIS_TOP_CELL, ART_ROW, fw, fh);

    // The stock atlas has blank placeholders where these would go.
    paint(art_chest_top, terrain, 10, 1, fw, fh);
    paint(art_chest_side, terrain, 11, 1, fw, fh);
    paint(art_chest_front, terrain, 9, 2, fw, fh);
    paint(art_smithing_top, terrain, 10, 2, fw, fh);
    paint(art_smithing_side, terrain, 9, 3, fw, fh);
    paint(art_smithing_front, terrain, 10, 3, fw, fh);
}
