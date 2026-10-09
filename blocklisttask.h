#ifndef BLOCKLISTTASK_H
#define BLOCKLISTTASK_H

#include "task.h"

class BlockListTask : public Task
{
public:
    BlockListTask();

    virtual void makeCurrent() override;

    virtual void render() override;
    virtual void logic() override;

    int current_selection;

private:
    static const int fields_x = 9, fields_y = 5;
    static const int panel_width = 176, panel_height = 18 + fields_y * 18 + 4 + 18 + 7;
};

extern BlockListTask block_list_task;

#endif // BLOCKLISTTASK_H
