#ifndef ITEMS_H
#define ITEMS_H

#include <cstdint>

#include "terrain.h"

// Item IDs are part of the save format: never renumber a released ID.
// Blocks keep their own IDs (0-255); everything else starts at 256.
typedef uint16_t ItemId;
constexpr ItemId FIRST_ITEM = 256;

enum ItemIds : ItemId {
    // Materials
    ITEM_STICK = 256, ITEM_COAL, ITEM_CHARCOAL, ITEM_IRON_INGOT, ITEM_GOLD_INGOT, ITEM_DIAMOND,
    ITEM_EMERALD, ITEM_REDSTONE_DUST, ITEM_NETHERITE_SCRAP, ITEM_NETHERITE_INGOT, ITEM_BONE,
    ITEM_ARROW, ITEM_GUNPOWDER, ITEM_LEATHER, ITEM_FEATHER, ITEM_STRING, ITEM_WHEAT, // ..272

    // Food
    ITEM_APPLE = 288, ITEM_BREAD, ITEM_PORK_RAW, ITEM_PORK_COOKED, ITEM_BEEF_RAW, ITEM_BEEF_COOKED,
    ITEM_CHICKEN_RAW, ITEM_CHICKEN_COOKED, ITEM_MUTTON_RAW, ITEM_MUTTON_COOKED, ITEM_ROTTEN_FLESH, // ..298

    // Tools occupy 320-343, see toolId()
    ITEM_FIRST_TOOL = 320,
    ITEM_BOW = 344,
    ITEM_LAST = 344
};

enum class ToolKind : uint8_t { NONE, PICKAXE, AXE, SHOVEL, SWORD, BOW };
enum class ToolMaterial : uint8_t { WOOD, STONE, IRON, GOLD, DIAMOND, NETHERITE };

// 320 + 4 * material + kind, kinds ordered pickaxe, axe, shovel, sword.
constexpr ItemId toolId(ToolMaterial m, ToolKind k)
{
    return ITEM_FIRST_TOOL + 4 * static_cast<int>(m) + (static_cast<int>(k) - static_cast<int>(ToolKind::PICKAXE));
}

struct ItemDef {
    const char *name;           // nullptr for blocks: their names come from the block renderer
    uint8_t max_stack;          // 0 = no such item
    ToolKind tool;
    uint8_t harvest_tier;       // 0 hand, 1 wood/gold, 2 stone, 3 iron, 4 diamond, 5 netherite
    uint8_t mining_speed;       // multiplier when the tool suits the block
    uint16_t durability;        // uses before breaking, 0 = never wears
    uint8_t attack_damage;      // 1.8.8 total, including the 1-point base hit
    uint8_t food_hunger;
    uint8_t food_saturation_x10;
};

const ItemDef &itemDef(ItemId id);
bool isBlockItem(ItemId id);

struct ItemStack {
    ItemId id;
    uint8_t count;
    uint16_t meta;  // block data for block items, wear for tools

    ItemStack(ItemId id = 0, uint8_t count = 0, uint16_t meta = 0) : id(id), count(count), meta(meta) {}

    bool empty() const { return count == 0; }
    static ItemStack ofBlock(BLOCK_WDATA b, uint8_t count = 1);
    BLOCK_WDATA toBlock() const;
    bool stacksWith(const ItemStack &o) const;
};

#endif // ITEMS_H
