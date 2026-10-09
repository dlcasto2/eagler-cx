#ifndef SETTINGSTASK_H
#define SETTINGSTASK_H

#include "task.h"

#include <vector>
#include <zlib.h>

class SettingsTask : public Task
{
public:
    struct SettingsEntry {
        const char *name;
        const char **values; //If nullptr, the numeric value is used
        unsigned int values_count;
        unsigned int current_value;
        unsigned int min_value; //Only makes sense if values == nullptr
        unsigned int step;
    };

    enum Settings {
        LEAVES = 0,
        SPEED,
        DISTANCE, //Managed by World, but can be changed here as well
        FAST_MODE,
        NEARPLANE_Z,
        TICKS_ENABLED,
        SHOW_FPS,
        AUTO_JUMP,
    };

    SettingsTask();
    virtual ~SettingsTask();

    virtual void makeCurrent() override;

    virtual void render() override;
    virtual void logic() override;

    unsigned int getValue(unsigned int entry) const;

    bool loadFromFile(gzFile file, int version);
    bool saveToFile(gzFile file);

private:
    std::vector<SettingsEntry> settings;
    // Rows: every setting, then Game Mode (kept in the world save, not here), then Done.
    unsigned int rowCount() const { return settings.size() + 2; }
    unsigned int gameModeRow() const { return settings.size(); }
    void change(int direction);
    void close();
    static constexpr int row_width = 200, row_height = 18, row_gap = 2, rows_top = 22;
public:
    unsigned int current_selection = 0;
private:
    bool changed_something;
};

extern SettingsTask settings_task;

#endif // SETTINGSTASK_H
