#include "entity.h"

#include "blockrenderer.h"
#include "item_icons.h"
#include "itempile.h"
#include "particle.h"
#include "terrain.h"
#include "texturetools.h"

namespace {

// nGL treats black as transparent in 3D, while the icon atlas uses magenta,
// so drops draw from a black-keyed copy made the first time it's needed.
const TEXTURE &iconAtlas3D()
{
    static TEXTURE *copy = nullptr;
    if(!copy)
    {
        const TEXTURE &src = itemIconAtlas();
        copy = newTexture(src.width, src.height, 0, true, 0);
        for(unsigned i = 0; i < static_cast<unsigned>(src.width) * src.height; ++i)
        {
            const COLOR c = src.bitmap[i];
            copy->bitmap[i] = c == src.transparent_color ? 0x0000 : (c == 0x0000 ? 0x0841 : c);
        }
    }
    return *copy;
}

constexpr int DROP_SIZE = BLOCK_SIZE * 3 / 10, PILE_SIZE = BLOCK_SIZE / 2;

void renderStack(const VECTOR3 &center, int size, const ItemStack &s)
{
    if(isBlockItem(s.id))
    {
        glBindTexture(terrain_current);
        Particle::render(center, size, global_block_renderer.materialTexture(s.toBlock()).current);
        return;
    }
    int u, v;
    if(itemIconUV(s.id, u, v))
    {
        glBindTexture(&iconAtlas3D());
        Particle::render(center, size, textureArea(u, v, 16, 16));
    }
}

} // namespace

void renderEntities(const EntityPool &pool)
{
    const TextureAtlasEntry &dummy_tex = block_textures[BLOCK_PUMPKIN][BLOCK_FRONT].current;

    pool.forEach([&dummy_tex](const Entity &e) {
        const VECTOR3 center{(e.box.low_x + e.box.high_x) / 2, (e.box.low_y + e.box.high_y) / 2, (e.box.low_z + e.box.high_z) / 2};

        if(e.type == EntityType::DROPPED_ITEM)
        {
            renderStack(center, DROP_SIZE, e.item);
            return;
        }
        if(e.type == EntityType::ITEM_PILE)
        {
            // A pile shows its first stack, a bit bigger than a single drop.
            if(ItemPile *p = item_piles.get(e.pile_index))
                for(const ItemStack &s : p->stacks)
                    if(!s.empty())
                    {
                        renderStack(center, PILE_SIZE, s);
                        break;
                    }
            return;
        }

        // Placeholder look until mobs arrive: the pumpkin face.
        glBindTexture(terrain_current);
        Particle::render(center, e.box.high_x - e.box.low_x, dummy_tex);
    });

    glBindTexture(terrain_current);
}
