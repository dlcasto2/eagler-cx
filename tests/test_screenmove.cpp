#include "doctest.h"
#include "screenmove.h"

#include <random>

static unsigned total(const ItemStack *slots, int n, const ItemStack &held, ItemId id)
{
    unsigned t = held.id == id ? held.count : 0;
    for(int i = 0; i < n; ++i)
        if(slots[i].id == id && !slots[i].empty())
            t += slots[i].count;
    return t;
}

TEST_CASE("take picks up, puts down, merges and swaps") {
    ItemStack s[3]; ItemStack held;
    s[0] = ItemStack(ITEM_COAL, 10);
    moveTake(s, 3, 0, held);
    CHECK(s[0].empty()); CHECK(held.id == ITEM_COAL); CHECK(held.count == 10);
    moveTake(s, 3, 1, held);
    CHECK(held.empty()); CHECK(s[1].count == 10);

    s[2] = ItemStack(ITEM_COAL, 60); held = ItemStack(ITEM_COAL, 10);
    moveTake(s, 3, 2, held);
    CHECK(s[2].count == 64); CHECK(held.count == 6);   // merge up to the stack limit

    held = ItemStack(ITEM_STICK, 3);
    moveTake(s, 3, 1, held);                          // different item: swap
    CHECK(s[1].id == ITEM_STICK); CHECK(s[1].count == 3);
    CHECK(held.id == ITEM_COAL); CHECK(held.count == 10);
}

TEST_CASE("half rounds up and leaves the rest") {
    ItemStack s[1]; ItemStack held;
    s[0] = ItemStack(ITEM_COAL, 7);
    moveHalf(s, 1, 0, held);
    CHECK(held.count == 4); CHECK(s[0].count == 3);
    s[0] = ItemStack(ITEM_COAL, 1); held = ItemStack();
    moveHalf(s, 1, 0, held);
    CHECK(held.count == 1); CHECK(s[0].empty());
}

TEST_CASE("place one into empty or matching slots only") {
    ItemStack s[2]; ItemStack held(ITEM_COAL, 3);
    movePlaceOne(s, 2, 0, held);
    CHECK(s[0].count == 1); CHECK(held.count == 2);
    movePlaceOne(s, 2, 0, held);
    CHECK(s[0].count == 2); CHECK(held.count == 1);
    s[1] = ItemStack(ITEM_COAL, 64);
    movePlaceOne(s, 2, 1, held);                      // full: nothing moves
    CHECK(s[1].count == 64); CHECK(held.count == 1);
    movePlaceOne(s, 2, 0, held);
    CHECK(held.empty()); CHECK(s[0].count == 3);
}

TEST_CASE("tools never merge") {
    const ItemId pick = toolId(ToolMaterial::IRON, ToolKind::PICKAXE);
    ItemStack s[1]; s[0] = ItemStack(pick, 1, 5); ItemStack held(pick, 1, 0);
    moveTake(s, 1, 0, held);
    CHECK(s[0].meta == 0); CHECK(held.meta == 5); CHECK(held.count == 1);
}

TEST_CASE("send-across merges then fills empties") {
    ItemStack a[2], b[3];
    a[0] = ItemStack(ITEM_COAL, 40);
    b[1] = ItemStack(ITEM_COAL, 50); b[0] = ItemStack(ITEM_STICK, 1);
    moveSendAcross(a, 2, 0, b, 3);
    CHECK(b[1].count == 64); CHECK(b[2].count == 26); CHECK(a[0].empty());

    // no room: whatever doesn't fit stays put
    ItemStack c[1]; c[0] = ItemStack(ITEM_STICK, 64);
    a[1] = ItemStack(ITEM_COAL, 5);
    moveSendAcross(a, 2, 1, c, 1);
    CHECK(a[1].count == 5);
}

TEST_CASE("moves conserve item counts") {
    std::mt19937 rng(7);
    ItemStack slots[36]; ItemStack held;
    const ItemId ids[3] = { ITEM_COAL, ITEM_STICK, toolId(ToolMaterial::STONE, ToolKind::AXE) };
    for(int i = 0; i < 36; i += 2)
        slots[i] = ItemStack(ids[i % 3], ids[i % 3] >= ITEM_FIRST_TOOL ? 1 : 1 + i);
    unsigned before[3];
    for(int k = 0; k < 3; ++k) before[k] = total(slots, 36, held, ids[k]);

    for(int step = 0; step < 5000; ++step)
    {
        const int i = static_cast<int>(rng() % 36), op = static_cast<int>(rng() % 4);
        if(op == 0) moveTake(slots, 36, i, held);
        else if(op == 1) { if(held.empty()) moveHalf(slots, 36, i, held); else movePlaceOne(slots, 36, i, held); }
        else if(op == 2) moveSendAcross(slots, 9, i % 9, slots + 9, 27);
        else moveSendAcross(slots + 9, 27, i % 27, slots, 9);
        for(int k = 0; k < 3; ++k)
            REQUIRE(total(slots, 36, held, ids[k]) == before[k]);
        for(auto &s : slots)
            REQUIRE((s.empty() || s.count <= itemDef(s.id).max_stack));
    }
}

TEST_CASE("closing returns the held stack") {
    ItemStack s[4]; ItemStack held;
    s[0] = ItemStack(ITEM_COAL, 60); s[1] = ItemStack(ITEM_STICK, 1);
    moveTake(s, 4, 1, held);                  // pick up the stick
    held = ItemStack(ITEM_COAL, 10); s[1] = ItemStack(ITEM_STICK, 1); // held coal now
    returnHeld(s, 4, held);
    CHECK(held.empty());
    CHECK(s[0].count == 64); CHECK(s[2].id == ITEM_COAL); CHECK(s[2].count == 6);
}
