#include "blockdefs.h"

namespace {

BlockDef table[256];

const BlockDrop SELF = { DROP_SELF, 1, 1, 1000 };
const BlockDrop NOTHING = { 0, 0, 0, 0 };

void def(BLOCK b, uint16_t hardness_x100, ToolKind tool, uint8_t tier, BlockDrop drop = SELF, uint8_t light = 0, bool blast_proof = false)
{
    table[b] = BlockDef{ true, hardness_x100, tool, tier, drop, light, blast_proof };
}

BlockDrop drops(ItemId item, uint8_t min = 1, uint8_t max = 1)
{
    return BlockDrop{ item, min, max, 1000 };
}

bool buildTable()
{
    const ToolKind PICK = ToolKind::PICKAXE, AXE = ToolKind::AXE, SHOVEL = ToolKind::SHOVEL,
                   SWORD = ToolKind::SWORD, NONE = ToolKind::NONE;

    def(BLOCK_STONE, 150, PICK, 1, drops(BLOCK_COBBLESTONE));
    def(BLOCK_COBBLESTONE, 200, PICK, 1);
    def(BLOCK_WALL, 150, PICK, 1);
    def(BLOCK_FURNACE, 350, PICK, 1);
    def(BLOCK_NETHERRACK, 40, PICK, 1);
    def(BLOCK_COAL_ORE, 300, PICK, 1, drops(ITEM_COAL));
    def(BLOCK_IRON_ORE, 300, PICK, 2);
    def(BLOCK_IRON, 500, PICK, 2);
    def(BLOCK_GOLD_ORE, 300, PICK, 3);
    def(BLOCK_GOLD, 300, PICK, 3);
    def(BLOCK_DIAMOND_ORE, 300, PICK, 3, drops(ITEM_DIAMOND));
    def(BLOCK_DIAMOND, 500, PICK, 3);
    def(BLOCK_REDSTONE_ORE, 300, PICK, 3, drops(ITEM_REDSTONE_DUST, 4, 5));
    def(BLOCK_PRESSURE_PLATE, 50, PICK, 1);

    def(BLOCK_DIRT, 50, SHOVEL, 0);
    def(BLOCK_SAND, 50, SHOVEL, 0);
    def(BLOCK_GRASS, 60, SHOVEL, 0, drops(BLOCK_DIRT));

    def(BLOCK_WOOD, 200, AXE, 0);
    def(BLOCK_PLANKS_NORMAL, 200, AXE, 0);
    def(BLOCK_PLANKS_DARK, 200, AXE, 0);
    def(BLOCK_PLANKS_BRIGHT, 200, AXE, 0);
    def(BLOCK_CRAFTING_TABLE, 250, AXE, 0);
    def(BLOCK_BOOKSHELF, 150, AXE, 0);
    def(BLOCK_PUMPKIN, 100, AXE, 0);
    def(BLOCK_DOOR, 300, AXE, 0);

    def(BLOCK_LEAVES, 20, NONE, 0, NOTHING);
    def(BLOCK_GLASS, 30, NONE, 0, NOTHING);
    def(BLOCK_GLOWSTONE, 30, NONE, 0, SELF, 15);
    def(BLOCK_REDSTONE_LAMP, 30, NONE, 0, SELF, 15);   // only while lit; checked in Phase 5
    def(BLOCK_SPONGE, 60, NONE, 0);
    def(BLOCK_SPIDERWEB, 400, SWORD, 1, drops(ITEM_STRING));
    def(BLOCK_CAKE, 50, NONE, 0, NOTHING);
    def(BLOCK_REDSTONE_SWITCH, 50, NONE, 0);
    def(BLOCK_TNT, 0, NONE, 0);
    def(BLOCK_TORCH, 0, NONE, 0, SELF, 14);
    def(BLOCK_FLOWER, 0, NONE, 0);
    def(BLOCK_MUSHROOM, 0, NONE, 0);
    def(BLOCK_REDSTONE_TORCH, 0, NONE, 0, SELF, 7);
    def(BLOCK_WHEAT, 0, NONE, 0, drops(ITEM_WHEAT));
    def(BLOCK_REDSTONE_WIRE, 0, NONE, 0, drops(ITEM_REDSTONE_DUST));

    def(BLOCK_BEDROCK, UNBREAKABLE, NONE, 0, NOTHING, 0, true);
    def(BLOCK_WATER, UNBREAKABLE, NONE, 0, NOTHING);
    def(BLOCK_LAVA, UNBREAKABLE, NONE, 0, NOTHING, 15);
    return true;
}

} // namespace

const BlockDef &blockDef(BLOCK b)
{
    static const bool built = buildTable();
    (void) built;
    return table[b];
}

ItemId dropItemFor(BLOCK b)
{
    const ItemId item = blockDef(b).drop.item;
    return item == DROP_SELF ? b : item;
}
