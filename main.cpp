#include <libndls.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "gl.h"
#include "terrain.h"
#include "worldtask.h"
#include "inventory.h"
#include "world.h"
#include "lighting.h"
#include "platform_time.h"
#include "gameclock.h"
#include "gamestate.h"
#include "playerinventory.h"

#include "textures/loading.h"

int main(int argc, char *argv[])
{
    #ifdef _TINSPIRE
        //Sometimes there's a clock on screen, switch that off
        __asm__ volatile("mrs r0, cpsr;"
                        "orr r0, r0, #0x80;"
                        "msr cpsr_c, r0;" ::: "r0");
    #endif

    nglInit();
    if(lcd_type() == SCR_320x240_4)
        greyscaleTexture(loading);
    nglSetBuffer(loading.bitmap);
    nglDisplay();

    //Early exit #1
    //(Task::keyPressed can only be used after initializeGlobals)
    if(isKeyPressed(KEY_NSPIRE_ESC))
    {
        nglUninit();
        return 0;
    }

    terrainInit("/documents/ndless/crafti.ppm.tns");
    lightingInit();
    glBindTexture(terrain_current);

    glLoadIdentity();

    //Early exit #2
    if(isKeyPressed(KEY_NSPIRE_ESC))
    {
        terrainUninit();
        nglUninit();

        return 0;
    }

    //If crafti has been started by the file extension association, use the first argument as savefile path
    Task::initializeGlobals(argc > 1 ? argv[1] : "/documents/ndless/crafti.map.tns");

    current_inventory.resetToDefaults();

    const LoadResult load_result = Task::load();
    switch(load_result)
    {
    case LoadResult::OK:
        world_task.setMessage("World loaded.");
        break;
    case LoadResult::MISSING:
        break;
    case LoadResult::UNREADABLE:
        Task::savefile = strdup(chooseSavePath(Task::savefile, load_result).c_str());
        world_task.setMessage("Save unreadable - kept. New world in .new");
        break;
    }


    #ifndef _TINSPIRE
        const bool verify_fixture = getenv("CRAFTI_VERIFY_FIXTURE") != nullptr;
        const bool save_on_exit = getenv("CRAFTI_SAVE_ON_EXIT") != nullptr;
    #endif

    //Start with WorldTask as current task
    world_task.makeCurrent();

    platformTimeInit();
    GameClock game_clock(platformTickRate());
    game_clock.reset(platformTicks());

    #ifndef _TINSPIRE
        const char *tpf_env = getenv("CRAFTI_TICKS_PER_FRAME");
        const unsigned long ticks_per_frame = tpf_env ? strtoul(tpf_env, nullptr, 10) : 0;
    #endif

    #ifndef _TINSPIRE
        // Headless runs: stop after CRAFTI_MAX_FRAMES frames, without saving.
        const char *max_frames_env = getenv("CRAFTI_MAX_FRAMES");
        const unsigned long max_frames = max_frames_env ? strtoul(max_frames_env, nullptr, 10) : 0;
        unsigned long frames_run = 0;
    #endif

    while(Task::running)
    {
        //Reset "loading" message
        drawLoadingtext(-1);

        Task::current_task->render();

        nglDisplay();

        Task::current_task->logic();

        #ifndef _TINSPIRE
            unsigned ticks = ticks_per_frame ? static_cast<unsigned>(ticks_per_frame) : game_clock.ticksDue(platformTicks());
        #else
            unsigned ticks = game_clock.ticksDue(platformTicks());
        #endif
        while(ticks-- > 0)
            Task::current_task->tick();

        #ifndef _TINSPIRE
            if(verify_fixture && frames_run == 10)
            {
                const char *fail = nullptr;
                if(player_inventory.slots[0].id != BLOCK_GOLD) fail = "slot0";
                else if(player_inventory.slots[1].id != BLOCK_TNT) fail = "slot1";
                else if(player_inventory.slots[2].id != BLOCK_GLASS) fail = "slot2";
                else if(player_inventory.slots[3].id != BLOCK_BOOKSHELF) fail = "slot3";
                else if(player_inventory.slots[4].id != BLOCK_PUMPKIN) fail = "slot4";
                else if(player_inventory.selected != 2) fail = "selected";
                else if(player_state.mode != GameMode::CREATIVE) fail = "mode";
                else if(world.getBlock(3, 30, 3) != BLOCK_DIAMOND) fail = "block1";
                else if(world.getBlock(-5, 20, 7) != BLOCK_GLASS) fail = "block2";
                if(fail) { printf("fixture-FAIL %s\n", fail); return 1; }
                printf("fixture-ok\n");
                return 0;
            }
            if(max_frames && ++frames_run >= max_frames)
                Task::running = false;
        #endif
    }

    #ifndef _TINSPIRE
        if(save_on_exit)
            Task::save();
    #endif

    platformTimeDeinit();

    Task::deinitializeGlobals();

    nglUninit();

    lightingUninit();
    terrainUninit();

    return 0;
}
