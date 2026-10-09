#ifndef SCREENMOVE_H
#define SCREENMOVE_H

#include "items.h"

// The slot-move rules of 1.8.8 inventory screens, independent of drawing.
// Every move conserves the total count of every item.

// Key 5 (left click): pick up, put down, merge up to the stack limit, or swap.
void moveTake(ItemStack *slots, int n, int index, ItemStack &held);
// Key 7 with empty hands (right click): pick up half, rounded up.
void moveHalf(ItemStack *slots, int n, int index, ItemStack &held);
// Key 7 while holding (right click): put down one if the slot is empty or matches.
// A different item swaps, as in 1.8.8.
void movePlaceOne(ItemStack *slots, int n, int index, ItemStack &held);
// Enter (shift-click): send from[index] to the other section, topping up
// matching stacks first, then filling empty slots. What doesn't fit stays.
void moveSendAcross(ItemStack *from, int fn, int index, ItemStack *to, int tn);
// Closing a screen: merge the held stack back, then into the first empty slot.
// Returns false (keeping the rest held) only if there is truly no room.
bool returnHeld(ItemStack *slots, int n, ItemStack &held);

#endif // SCREENMOVE_H
