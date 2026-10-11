#include "bedrenderer.h"

#include "containers.h"

constexpr GLFix BedRenderer::bed_height;

namespace {

// Atlas cells of the bundled pack's bed.
constexpr int TOP_FOOT_X = 6, TOP_HEAD_X = 7, TOP_Y = 8;
constexpr int END_FOOT_X = 5, SIDE_FOOT_X = 6, SIDE_HEAD_X = 7, END_HEAD_X = 8, SIDE_Y = 9;

// Only the lower 9/16 of a side texture shows, like the cake.
TextureAtlasEntry lowerPart(TextureAtlasEntry t)
{
    t.top = t.top + (t.bottom - t.top) * 7 / 16;
    return t;
}

} // namespace

void BedRenderer::renderSpecialBlock(const BLOCK_WDATA block, GLFix x, GLFix y, GLFix z, Chunk &c)
{
    const uint8_t data = getBLOCKDATA(block);
    const BLOCK_SIDE facing = bedFacing(data);
    const bool head = data & BED_HEAD;
    const GLFix h = bed_height, s = BLOCK_SIZE;

    // ---- top: the texture's left-to-right runs from foot to head
    const TextureAtlasEntry top = terrain_atlas[head ? TOP_HEAD_X : TOP_FOOT_X][TOP_Y].current;
    auto uvAt = [&](int cx, int cz, GLFix &u, GLFix &v) {
        int along = 0, across = 0;   // 0 or 1
        switch(facing)
        {
        case BLOCK_RIGHT: along = cx;     across = cz;     break;
        case BLOCK_LEFT:  along = 1 - cx; across = 1 - cz; break;
        case BLOCK_BACK:  along = cz;     across = 1 - cx; break;
        default:          along = 1 - cz; across = cx;     break;   // BLOCK_FRONT
        }
        u = along ? top.right : top.left;
        v = across ? top.top : top.bottom;
    };
    GLFix u, v;
    uvAt(0, 0, u, v); c.addUnalignedVertex({x, y + h, z, u, v, TEXTURE_TRANSPARENT});
    uvAt(0, 1, u, v); c.addUnalignedVertex({x, y + h, z + s, u, v, TEXTURE_TRANSPARENT});
    uvAt(1, 1, u, v); c.addUnalignedVertex({x + s, y + h, z + s, u, v, TEXTURE_TRANSPARENT});
    uvAt(1, 0, u, v); c.addUnalignedVertex({x + s, y + h, z, u, v, TEXTURE_TRANSPARENT});

    // ---- the four sides: long sides everywhere, an end only on the outer end
    const TextureAtlasEntry side = lowerPart(terrain_atlas[head ? SIDE_HEAD_X : SIDE_FOOT_X][SIDE_Y].current);
    const TextureAtlasEntry end = lowerPart(terrain_atlas[head ? END_HEAD_X : END_FOOT_X][SIDE_Y].current);
    const BLOCK_SIDE outer = head ? facing : oppositeSide(facing);
    const BLOCK_SIDE inner = head ? oppositeSide(facing) : facing;

    auto texFor = [&](BLOCK_SIDE face) -> const TextureAtlasEntry * {
        if(face == inner) return nullptr;     // hidden against the other half
        return face == outer ? &end : &side;
    };

    if(const TextureAtlasEntry *t = texFor(BLOCK_FRONT))   // -z
    {
        c.addUnalignedVertex({x, y, z, t->left, t->bottom, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x, y + h, z, t->left, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x + s, y + h, z, t->right, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x + s, y, z, t->right, t->bottom, TEXTURE_TRANSPARENT});
    }
    if(const TextureAtlasEntry *t = texFor(BLOCK_BACK))    // +z
    {
        c.addUnalignedVertex({x + s, y, z + s, t->left, t->bottom, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x + s, y + h, z + s, t->left, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x, y + h, z + s, t->right, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x, y, z + s, t->right, t->bottom, TEXTURE_TRANSPARENT});
    }
    if(const TextureAtlasEntry *t = texFor(BLOCK_LEFT))    // -x
    {
        c.addUnalignedVertex({x, y, z + s, t->left, t->bottom, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x, y + h, z + s, t->left, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x, y + h, z, t->right, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x, y, z, t->right, t->bottom, TEXTURE_TRANSPARENT});
    }
    if(const TextureAtlasEntry *t = texFor(BLOCK_RIGHT))   // +x
    {
        c.addUnalignedVertex({x + s, y, z, t->left, t->bottom, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x + s, y + h, z, t->left, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x + s, y + h, z + s, t->right, t->top, TEXTURE_TRANSPARENT});
        c.addUnalignedVertex({x + s, y, z + s, t->right, t->bottom, TEXTURE_TRANSPARENT});
    }
}

void BedRenderer::geometryNormalBlock(const BLOCK_WDATA /*block*/, const int local_x, const int local_y, const int local_z, const BLOCK_SIDE side, Chunk &c)
{
    if(side == BLOCK_BOTTOM)
        renderNormalBlockSide(local_x, local_y, local_z, side, terrain_atlas[4][0].current, c);   // planks underneath
}

AABB BedRenderer::getAABB(const BLOCK_WDATA /*block*/, GLFix x, GLFix y, GLFix z)
{
    return {x, y, z, x + BLOCK_SIZE, y + bed_height, z + BLOCK_SIZE};
}

void BedRenderer::drawPreview(const BLOCK_WDATA /*block*/, TEXTURE &dest, int x, int y)
{
    BlockRenderer::drawTextureAtlasEntry(*terrain_resized, terrain_atlas[TOP_HEAD_X][TOP_Y].resized, dest, x, y);
}
