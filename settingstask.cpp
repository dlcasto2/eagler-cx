#include "settingstask.h"

#include "gamestate.h"
#include "uikit.h"
#include "texturetools.h"
#include "worldtask.h"


SettingsTask settings_task;

const char *leaves_values[] = {
    "Opaque",
    "Transparent"
};

const char *speed_values[] = {
    "Slow",
    "Normal",
    "Fast"
};

const char *fastmode_values[] = {
    "Off",
    "On"
};

const char *world_static_values[] = {
    "Static (no ticks)",
    "Dynamic"
};

SettingsTask::SettingsTask()
{
    //Must have the same order as the "Settings" enum
    //When changing something in an incompatible way (order, meaning),
    //increment savefile_version in task.cpp and add handling to SettingsTask::loadFromFile.
    //Not needed when just adding or removing entries at the end, except when adding after
    //removing entries in the past.
    settings.push_back({"Leaves", leaves_values, 2, 0, 0, 1});
    settings.push_back({"Speed", speed_values, 3, 1, 0, 1});
    settings.push_back({"Distance", nullptr, 10, 2, 1, 1});
    settings.push_back({"Fast mode", fastmode_values, 2, 0, 0, 1});
    settings.push_back({"Near plane", nullptr, 512+1, 256, 128, 16});
    settings.push_back({"World", world_static_values, 2, 1, 0, 1});
    settings.push_back({"Show FPS", fastmode_values, 2, 0, 0, 1});
    settings.push_back({"Auto-jump", fastmode_values, 2, 1, 0, 1}); //On by default
}

SettingsTask::~SettingsTask()
{
}

void SettingsTask::makeCurrent()
{
    if(!background_saved)
        saveBackground();

    settings[DISTANCE].current_value = world.fieldOfView();

    changed_something = false;

    Task::makeCurrent();
}

void SettingsTask::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    drawPixelTextCenter(*screen, "Options", SCREEN_WIDTH / 2, 8, ui::TEXT_WHITE);

    const int x = (SCREEN_WIDTH - row_width) / 2;
    int y = rows_top;
    char label[48];

    for(unsigned int i = 0; i < rowCount(); ++i, y += row_height + row_gap)
    {
        bool enabled = true;
        if(i < settings.size())
        {
            const SettingsEntry &e = settings[i];
            if(e.values == nullptr)
                snprintf(label, sizeof(label), "%s: %u", e.name, e.current_value);
            else
                snprintf(label, sizeof(label), "%s: %s", e.name, e.values[e.current_value]);
        }
        else if(i == gameModeRow())
        {
            snprintf(label, sizeof(label), "Game Mode: %s", player_state.mode == GameMode::SURVIVAL ? "Survival" : "Creative");
            enabled = canChangeGameMode(world_state.hardcore);
        }
        else
            snprintf(label, sizeof(label), "Done");

        drawButton(*screen, x, y, row_width, row_height, label, i == current_selection, enabled);
    }

    drawPixelText(*screen, "8/2 move   4/6 change   esc done", 3, SCREEN_HEIGHT - 11, ui::GREY_TEXT);
}

void SettingsTask::close()
{
    world_task.makeCurrent();

    if(changed_something)
    {
        world.setDirty();
        world.setFieldOfView(settings[DISTANCE].current_value);

        nglSetNearPlane(settings[NEARPLANE_Z].current_value);

        world_task.setMessage("Settings applied.");
    }
}

void SettingsTask::change(int direction)
{
    if(current_selection == gameModeRow())
    {
        if(canChangeGameMode(world_state.hardcore))
            player_state.mode = player_state.mode == GameMode::SURVIVAL ? GameMode::CREATIVE : GameMode::SURVIVAL;
        return;
    }
    if(current_selection >= settings.size())
        return;

    SettingsEntry &entry = settings[current_selection];
    if(direction < 0)
    {
        if(entry.current_value < entry.min_value + entry.step)
            entry.current_value = entry.values_count - 1;
        else
            entry.current_value -= entry.step;
    }
    else
    {
        entry.current_value += entry.step;
        if(entry.current_value >= entry.values_count)
            entry.current_value = entry.min_value;
    }

    changed_something = true;
}

void SettingsTask::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN) || keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_4) || keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_6) || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    key_held_down = true;
    if(keyPressed(KEY_NSPIRE_ESC))
        close();
    else if(keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_8))
        current_selection = (current_selection + rowCount() - 1) % rowCount();
    else if(keyPressed(KEY_NSPIRE_DOWN) || keyPressed(KEY_NSPIRE_2))
        current_selection = (current_selection + 1) % rowCount();
    else if(keyPressed(KEY_NSPIRE_LEFT) || keyPressed(KEY_NSPIRE_4))
        change(-1);
    else if(keyPressed(KEY_NSPIRE_RIGHT) || keyPressed(KEY_NSPIRE_6))
        change(1);
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
    {
        // Like clicking a button: Done closes, anything else steps forward.
        if(current_selection == rowCount() - 1)
            close();
        else
            change(1);
    }
    else
        key_held_down = false;
}

unsigned int SettingsTask::getValue(unsigned int entry) const
{
    return settings[entry].current_value;
}

bool SettingsTask::loadFromFile(gzFile file, int version)
{
    // If some setting wasn't loaded, it keeps the current value.

    // Previous versions didn't have settings yet
    if(version < 5)
        return true;

    //World doesn't care about DISTANCE being saved and loaded here as well

    unsigned int entries_in_file;
    if(gzfread(&entries_in_file, sizeof(entries_in_file), 1, file) != 1)
        return false;

    for(unsigned int i = 0; i < entries_in_file; ++i)
    {
        unsigned int value;
        if(gzfread(&value, sizeof(unsigned int), 1, file) != 1)
            return false;

        if(i < settings.size()
            && value >= settings[i].min_value
            && value < settings[i].values_count)
            settings[i].current_value = value;
    }

    world.setDirty();

    nglSetNearPlane(settings[NEARPLANE_Z].current_value);

    return true;
}

bool SettingsTask::saveToFile(gzFile file)
{
    unsigned int size = settings.size();
    if(gzfwrite(&size, sizeof(size), 1, file) != 1)
        return false;

    for(unsigned int i = 0; i < size; ++i)
    {
        if(gzfwrite(&settings[i].current_value, sizeof(unsigned int), 1, file) != 1)
            return false;
    }

    return true;
}
