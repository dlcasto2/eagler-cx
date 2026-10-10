#ifndef WORLDTASK_H
#define WORLDTASK_H

#include "task.h"
#include "world.h"
#include "gl.h"
#include "aabb.h"

class WorldTask : public Task
{
public:
    virtual void makeCurrent() override;

    virtual void logic() override;
    virtual void tick() override;
    virtual void render() override;

    void resetWorld();

    GLFix x, y = World::HEIGHT * Chunk::SIZE * BLOCK_SIZE, z, xr, yr;

    static constexpr GLFix player_width = BLOCK_SIZE*0.8f, player_height = BLOCK_SIZE*1.8f, eye_pos = BLOCK_SIZE*1.6f;

    void setMessage(const char *message);

    unsigned int frameCount() { return frame_counter; }

private:
    void crosshairPixel(int x, int y);
    void renderSky();

    void getForward(GLFix *x, GLFix *z);
    void getRight(GLFix *x, GLFix *z);

    GLFix speed();

    //Player position and movement
    AABB aabb;
    bool can_jump = false, tp_had_contact = false;
    int tp_last_x = 0, tp_last_y = 0;
    GLFix vy = 0; //Y-Velocity for gravity and jumps
    bool in_water = false;

    int8_t input_forward = 0, input_strafe = 0; //Set by logic(), used by tick()
    bool input_jump = false;

    #ifdef STRESS_TEST
        bool stress_spawned = false;
        uint32_t stress_tick_count = 0, stress_tps = 0, stress_ticks_this_second = 0, stress_rtc_second = 0;
    #endif

    static constexpr unsigned int blockselection_frames = 2;
    unsigned int blockselection_frame = 0, blockselection_frame_fraction = 0;

    VECTOR3 selection_pos; AABB::SIDE selection_side; VECTOR3 selection_pos_abs; bool do_test = true; //For intersectsRay

    char message[40]; unsigned int message_timeout = 0;

    bool draw_inventory = true;
    unsigned int frame_counter = 0; // Incremented after each render
};

extern WorldTask world_task;

#endif // WORLDTASK_H
