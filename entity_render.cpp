#include "entity.h"

#include "particle.h"
#include "terrain.h"

// Placeholder look until Phase 6: every entity is a billboard of the pumpkin face.
void renderEntities(const EntityPool &pool)
{
    glBindTexture(terrain_current);
    const TextureAtlasEntry &tex = block_textures[BLOCK_PUMPKIN][BLOCK_FRONT].current;

    pool.forEach([&tex](const Entity &e) {
        const VECTOR3 center{(e.box.low_x + e.box.high_x) / 2, (e.box.low_y + e.box.high_y) / 2, (e.box.low_z + e.box.high_z) / 2};
        Particle::render(center, e.box.high_x - e.box.low_x, tex);
    });
}
