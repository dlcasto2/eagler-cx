#ifndef HELPTASK_H
#define HELPTASK_H

#include "task.h"

#include "gl.h"

// Lists every control, with a Done button.
class HelpTask : public Task
{
public:
    virtual void makeCurrent() override;

    virtual void render() override;
    virtual void logic() override;
};

extern HelpTask help_task;

#endif // HELPTASK_H
