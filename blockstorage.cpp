#include "blockstorage.h"

BlockStorage block_storage;

bool operator<(const BlockPos &a, const BlockPos &b)
{
    if(a.x != b.x) return a.x < b.x;
    if(a.y != b.y) return a.y < b.y;
    return a.z < b.z;
}

static int slotCount(StorageKind kind)
{
    return kind == StorageKind::FURNACE ? FURNACE_SLOTS : CHEST_SLOTS;
}

StorageRecord *BlockStorage::find(const BlockPos &p)
{
    auto it = records.find(p);
    return it == records.end() ? nullptr : &it->second;
}

StorageRecord &BlockStorage::create(const BlockPos &p, StorageKind kind)
{
    StorageRecord &r = records[p];
    r = StorageRecord{ kind, std::vector<ItemStack>(slotCount(kind)), 0, 0, 0 };
    return r;
}

void BlockStorage::erase(const BlockPos &p)
{
    records.erase(p);
}

// u32 count; per record: i32 x, y, z; u8 kind; its slots; u16 burn left, burn total, cook ticks.
bool BlockStorage::save(SaveWriter &w) const
{
    w.u32(static_cast<uint32_t>(records.size()));
    for(const auto &entry : records)
    {
        const StorageRecord &r = entry.second;
        w.i32(entry.first.x); w.i32(entry.first.y); w.i32(entry.first.z);
        w.u8(static_cast<uint8_t>(r.kind));
        for(const ItemStack &s : r.slots)
            w.stack(s);
        w.u16(r.burn_ticks_left); w.u16(r.burn_ticks_total); w.u16(r.cook_ticks);
    }
    return w.ok();
}

bool BlockStorage::load(SaveReader &r)
{
    records.clear();

    uint32_t count;
    if(!r.u32(count))
        return false;

    for(uint32_t i = 0; i < count; ++i)
    {
        BlockPos p;
        uint8_t kind;
        if(!r.i32(p.x) || !r.i32(p.y) || !r.i32(p.z) || !r.u8(kind))
            return false;
        if(kind != static_cast<uint8_t>(StorageKind::FURNACE) && kind != static_cast<uint8_t>(StorageKind::CHEST))
            return false;

        StorageRecord &rec = create(p, static_cast<StorageKind>(kind));
        for(ItemStack &s : rec.slots)
            if(!r.stack(s))
                return false;
        if(!r.u16(rec.burn_ticks_left) || !r.u16(rec.burn_ticks_total) || !r.u16(rec.cook_ticks))
            return false;
    }
    return true;
}
