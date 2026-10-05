#include "savefile.h"

#include <cstdio>

#include "blockstorage.h"
#include "itempile.h"

bool writeSurvivalSection(SaveWriter &w)
{
    w.u8(static_cast<uint8_t>(player_state.mode));
    w.u8(static_cast<uint8_t>(world_state.difficulty));
    w.u8(world_state.hardcore ? 1 : 0);
    w.u8(world_state.hardcore_dead ? 1 : 0);

    w.u32(world_state.time_of_day);
    w.u32(world_state.trader_next_visit);

    w.u8(player_state.health);
    w.u8(player_state.hunger);
    w.u16(player_state.saturation_x100);
    w.u16(player_state.exhaustion_x1000);
    w.u16(player_state.air_ticks);
    w.u16(player_state.burning_ticks);
    w.i32(player_state.spawn_x);
    w.i32(player_state.spawn_y);
    w.i32(player_state.spawn_z);
    w.u8(player_state.spawn_is_bed ? 1 : 0);

    w.u8(static_cast<uint8_t>(player_inventory.selected));
    for(const ItemStack &s : player_inventory.slots)
        w.stack(s);

    block_storage.save(w);
    item_piles.save(w);
    return w.ok();
}

bool readSurvivalSection(SaveReader &r)
{
    uint8_t mode, difficulty, hardcore, hardcore_dead;
    if(!r.u8(mode) || !r.u8(difficulty) || !r.u8(hardcore) || !r.u8(hardcore_dead))
        return false;
    player_state.mode = static_cast<GameMode>(mode);
    world_state.difficulty = static_cast<Difficulty>(difficulty);
    world_state.hardcore = hardcore != 0;
    world_state.hardcore_dead = hardcore_dead != 0;

    if(!r.u32(world_state.time_of_day) || !r.u32(world_state.trader_next_visit))
        return false;

    if(!r.u8(player_state.health) || !r.u8(player_state.hunger)
       || !r.u16(player_state.saturation_x100) || !r.u16(player_state.exhaustion_x1000)
       || !r.u16(player_state.air_ticks) || !r.u16(player_state.burning_ticks)
       || !r.i32(player_state.spawn_x) || !r.i32(player_state.spawn_y) || !r.i32(player_state.spawn_z))
        return false;
    uint8_t spawn_is_bed;
    if(!r.u8(spawn_is_bed))
        return false;
    player_state.spawn_is_bed = spawn_is_bed != 0;

    uint8_t selected;
    if(!r.u8(selected))
        return false;
    player_inventory.selected = selected;
    for(ItemStack &s : player_inventory.slots)
        if(!r.stack(s))
            return false;

    return block_storage.load(r) && item_piles.load(r);
}

void convertV6Inventory(const BLOCK_WDATA (&entries)[5], int current_slot, PlayerInventory &inv, PlayerState &ps)
{
    inv.clear();
    for(int i = 0; i < 5; ++i)
        inv.slots[i] = ItemStack::ofBlock(entries[i]);
    inv.selected = (current_slot >= 0 && current_slot < 5) ? current_slot : 0;
    ps.mode = GameMode::CREATIVE;
}

static std::string withSuffix(const std::string &path, const char *suffix)
{
    const std::string tail = ".map.tns";
    if(path.size() >= tail.size() && path.compare(path.size() - tail.size(), tail.size(), tail) == 0)
        return path.substr(0, path.size() - tail.size()) + ".map." + suffix + ".tns";
    return path + "." + suffix;
}

std::string backupPathFor(const std::string &path) { return withSuffix(path, "bak"); }
std::string tempPathFor(const std::string &path) { return withSuffix(path, "tmp"); }
std::string newWorldPathFor(const std::string &path) { return withSuffix(path, "new"); }

std::string chooseSavePath(const std::string &original, LoadResult r)
{
    return r == LoadResult::UNREADABLE ? newWorldPathFor(original) : original;
}

int readSaveVersion(const char *path)
{
    gzFile f = gzopen(path, "rb");
    if(!f)
        return -1;
    int version;
    const int read = gzread(f, &version, sizeof(version));
    gzclose(f);
    return read == sizeof(version) ? version : -2;
}

bool replaceSaveSafely(const std::string &path, bool (*write)(gzFile, void *), void *ctx)
{
    const std::string temp = tempPathFor(path);

    gzFile f = gzopen(temp.c_str(), "wb");
    if(!f)
        return false;

    const bool written = write(f, ctx);
    const bool closed_ok = gzclose(f) == Z_OK;
    if(!written || !closed_ok)
    {
        remove(temp.c_str());
        return false;
    }

    const int existing = readSaveVersion(path.c_str());
    const std::string backup = backupPathFor(path);
    if(existing >= 0 && existing < 7 && readSaveVersion(backup.c_str()) == -1)
        if(rename(path.c_str(), backup.c_str()) != 0)
        {
            remove(temp.c_str());
            return false;
        }

    if(rename(temp.c_str(), path.c_str()) != 0)
    {
        remove(path.c_str());
        if(rename(temp.c_str(), path.c_str()) != 0)
            return false;
    }
    return true;
}
