#ifndef BLOCKDEFS_H
#define BLOCKDEFS_H

#include <cstdint>

#include "items.h"
#include "terrain.h"

// Mining and lighting properties per block, 1.8.8 values.
//

constexpr uint16_t UNBREAKABLE = 0xFFFF;
constexpr ItemId DROP_SELF = 0xFFFF;

struct BlockDrop {
    ItemId item;                // 0 = drops nothing, DROP_SELF = the block itself
    uint8_t min, max;
    uint16_t chance_permille;
};

struct BlockDef {
    bool defined;
    uint16_t hardness_x100;     // UNBREAKABLE for bedrock and fluids
    ToolKind best_tool;
    uint8_t min_tier;           // tool tier needed for a drop, 0 = by hand
    BlockDrop drop;
    uint8_t light;              // light emitted, 0-15
    bool blast_proof;
};

const BlockDef &blockDef(BLOCK b);
ItemId dropItemFor(BLOCK b);

#endif // BLOCKDEFS_H
