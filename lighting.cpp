#include "lighting.h"

#include <cstdlib>

#include "terrain.h"

namespace {

// Levels 0..6 are built copies; level 7 points at the original.
TEXTURE *dark_terrain[BRIGHTNESS_LEVELS] = {};
TEXTURE *dark_quad[BRIGHTNESS_LEVELS] = {};
size_t memory_bytes = 0;

TEXTURE *buildCopy(const TEXTURE *src, int level)
{
    TEXTURE *copy = static_cast<TEXTURE*>(malloc(sizeof(TEXTURE)));
    if(!copy)
        return nullptr;

    const size_t pixels = static_cast<size_t>(src->width) * src->height;
    copy->width = src->width;
    copy->height = src->height;
    copy->has_transparency = src->has_transparency;
    copy->transparent_color = src->transparent_color;
    copy->bitmap = static_cast<COLOR*>(malloc(pixels * sizeof(COLOR)));
    if(!copy->bitmap)
    {
        free(copy);
        return nullptr;
    }

    for(size_t i = 0; i < pixels; ++i)
        copy->bitmap[i] = darkenColor(src->bitmap[i], level, src->transparent_color);

    memory_bytes += sizeof(TEXTURE) + pixels * sizeof(COLOR);
    return copy;
}

} // namespace

bool lightingInit()
{
    dark_terrain[BRIGHTNESS_LEVELS - 1] = terrain_current;
    dark_quad[BRIGHTNESS_LEVELS - 1] = terrain_quad;

    for(int level = 0; level < BRIGHTNESS_LEVELS - 1; ++level)
    {
        dark_terrain[level] = buildCopy(terrain_current, level);
        dark_quad[level] = buildCopy(terrain_quad, level);
        if(!dark_terrain[level] || !dark_quad[level])
        {
            lightingUninit();
            return false;
        }
    }
    return true;
}

void lightingUninit()
{
    for(int level = 0; level < BRIGHTNESS_LEVELS - 1; ++level)
    {
        if(dark_terrain[level]) { free(dark_terrain[level]->bitmap); free(dark_terrain[level]); dark_terrain[level] = nullptr; }
        if(dark_quad[level]) { free(dark_quad[level]->bitmap); free(dark_quad[level]); dark_quad[level] = nullptr; }
    }
    memory_bytes = 0;
}

TEXTURE *terrainAtLevel(int level)
{
    if(level < 0) level = 0;
    if(level >= BRIGHTNESS_LEVELS) level = BRIGHTNESS_LEVELS - 1;
    return dark_terrain[level] ? dark_terrain[level] : terrain_current;
}

TEXTURE *terrainQuadAtLevel(int level)
{
    if(level < 0) level = 0;
    if(level >= BRIGHTNESS_LEVELS) level = BRIGHTNESS_LEVELS - 1;
    return dark_quad[level] ? dark_quad[level] : terrain_quad;
}

size_t lightingMemoryBytes()
{
    return memory_bytes;
}
