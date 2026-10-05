#ifndef BLOCKSTORAGE_H
#define BLOCKSTORAGE_H

#include <cstdint>
#include <map>
#include <vector>

#include "items.h"
#include "saveio.h"

struct BlockPos {
    int32_t x, y, z;
};

bool operator<(const BlockPos &a, const BlockPos &b);

enum class StorageKind : uint8_t { FURNACE = 1, CHEST = 2 };
constexpr int FURNACE_SLOTS = 3, CHEST_SLOTS = 27;

// Contents of a furnace or chest, kept apart from the block itself.
struct StorageRecord {
    StorageKind kind;
    std::vector<ItemStack> slots;
    uint16_t burn_ticks_left, burn_ticks_total, cook_ticks;
};

class BlockStorage
{
public:
    StorageRecord *find(const BlockPos &p);
    StorageRecord &create(const BlockPos &p, StorageKind kind);   // replaces any record at p
    void erase(const BlockPos &p);
    size_t size() const { return records.size(); }
    void clear() { records.clear(); }

    bool save(SaveWriter &w) const;
    bool load(SaveReader &r);

private:
    std::map<BlockPos, StorageRecord> records;
};

extern BlockStorage block_storage;

#endif // BLOCKSTORAGE_H
