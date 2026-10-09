#include "task.h"
#include <zlib.h>

#include "texturetools.h"
#include "blocklisttask.h"
#include "worldtask.h"
#include "settingstask.h"
#include "inventory.h"
#include "playerinventory.h"
#include "gamestate.h"

//The values have to stay somewhere
Task *Task::current_task;
bool Task::key_held_down, Task::running, Task::background_saved, Task::has_touchpad, Task::keys_inverted;
TEXTURE *Task::screen, *Task::background;
const char *Task::savefile;

void Task::makeCurrent()
{
    current_task = this;
}

bool Task::keyPressed(const t_key &key)
{
    #ifdef _TINSPIRE
        if(has_touchpad)
        {
            if(key.tpad_arrow != TPAD_ARROW_NONE)
                return touchpad_arrow_pressed(key.tpad_arrow);
            else
                return !(*reinterpret_cast<volatile uint16_t*>(0x900E0000 + key.tpad_row) & key.tpad_col) == keys_inverted;
        }
        else
            return (*reinterpret_cast<volatile uint16_t*>(0x900E0000 + key.row) & key.col) == 0;
    #else
        return false;
    #endif
}

void Task::initializeGlobals(const char *savefile)
{
    running = true;

    screen = newTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    nglSetBuffer(screen->bitmap);

    has_touchpad = is_touchpad;
    keys_inverted = is_classic;

    background = newTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    background_saved = false;

    Task::savefile = savefile;
}

void Task::deinitializeGlobals()
{
    deleteTexture(screen);
    deleteTexture(background);
}

void Task::saveBackground()
{
    copyTexture(*screen, *background);

    background_saved = true;
}

void Task::drawBackground()
{
    copyTexture(*background, *screen);
}

/* Version 2: First in git
 * Version 3 (31d5ee3a): Blocks are stored as 16bit BLOCK_WDATA
 * Version 4 (1ebc685a): Add inventory
 * Version 5 (710e7269): Add settings
 * Version 6 (d52f3992): BLOCK_SIZE changed from 120 to 128,
 *                       gzip compression introduced shortly afterwards
 */
static constexpr int savefile_version = 7;

#define LOAD_FROM_FILE(var) if(gzfread(&var, sizeof(var), 1, file) != 1) { gzclose(file); return false; }
#define SAVE_TO_FILE(var) if(gzfwrite(&var, sizeof(var), 1, file) != 1) { gzclose(file); return false; }

LoadResult Task::load()
{
    gzFile file = gzopen(savefile, "rb");
    if(!file)
        return LoadResult::MISSING;

    int version;
    if(gzread(file, &version, sizeof(version)) != sizeof(version))
    {
        gzclose(file);
        return LoadResult::UNREADABLE;
    }

    static_assert(savefile_version == 7, "Adjust loading code for backward compatibility");

    if(version < 4 || version > 7)
    {
        printf("Save file version %d not supported!\n", version);
        gzclose(file);
        return LoadResult::UNREADABLE;
    }

    #define LOAD_OR_FAIL(var) if(gzfread(&var, sizeof(var), 1, file) != 1) { gzclose(file); return LoadResult::UNREADABLE; }

    if(!settings_task.loadFromFile(file, version))
    {
        gzclose(file);
        return LoadResult::UNREADABLE;
    }

    if(version <= 6)
    {
        BLOCK_WDATA hotbar[5]; // v6 saves held a 5-slot hotbar
        LOAD_OR_FAIL(hotbar)
        LOAD_OR_FAIL(world_task.xr)
        LOAD_OR_FAIL(world_task.yr)
        LOAD_OR_FAIL(world_task.x)
        LOAD_OR_FAIL(world_task.y)
        LOAD_OR_FAIL(world_task.z)
        if(version < 6)
        {
            world_task.x = world_task.x * BLOCK_SIZE / 120;
            world_task.y = world_task.y * BLOCK_SIZE / 120;
            world_task.z = world_task.z * BLOCK_SIZE / 120;
        }
        int hotbar_slot;
        LOAD_OR_FAIL(hotbar_slot)
        convertV6Inventory(hotbar, hotbar_slot, player_inventory, player_state);
    }
    else // version 7
    {
        LOAD_OR_FAIL(world_task.x)
        LOAD_OR_FAIL(world_task.y)
        LOAD_OR_FAIL(world_task.z)
        LOAD_OR_FAIL(world_task.xr)
        LOAD_OR_FAIL(world_task.yr)

        SaveReader reader(file);
        if(!readSurvivalSection(reader))
        {
            gzclose(file);
            return LoadResult::UNREADABLE;
        }
    }

    LOAD_OR_FAIL(block_list_task.current_selection)

    const bool ret = world.loadFromFile(file);

    gzclose(file);

    world.setPosition(world_task.x, world_task.y, world_task.z);

    #undef LOAD_OR_FAIL

    return ret ? LoadResult::OK : LoadResult::UNREADABLE;
}

bool Task::save()
{
    struct Ctx {} ctx;

    return replaceSaveSafely(savefile, [](gzFile file, void *) -> bool {
        const int version = savefile_version;
        if(gzwrite(file, &version, sizeof(version)) != sizeof(version))
            return false;
        if(!settings_task.saveToFile(file))
            return false;

        #define SAVE_OR_FAIL(var) if(gzfwrite(&var, sizeof(var), 1, file) != 1) return false;
        SAVE_OR_FAIL(world_task.x)
        SAVE_OR_FAIL(world_task.y)
        SAVE_OR_FAIL(world_task.z)
        SAVE_OR_FAIL(world_task.xr)
        SAVE_OR_FAIL(world_task.yr)
        #undef SAVE_OR_FAIL

        SaveWriter writer(file);
        if(!writeSurvivalSection(writer))
            return false;

        if(gzfwrite(&block_list_task.current_selection, sizeof(block_list_task.current_selection), 1, file) != 1)
            return false;

        return world.saveToFile(file);
    }, &ctx);

}
