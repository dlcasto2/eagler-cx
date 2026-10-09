#ifndef HOTBAR_MODULE_H
#define HOTBAR_MODULE_H

#include "gl.h"

// The 182x22 nine-slot hotbar, centered at the bottom of the screen.
constexpr int HOTBAR_W = 182, HOTBAR_H = 22;
constexpr int HOTBAR_X = (SCREEN_WIDTH - HOTBAR_W) / 2, HOTBAR_Y = SCREEN_HEIGHT - HOTBAR_H - 1;

void drawHotbar(TEXTURE &dst);

// Hearts and hunger above the hotbar (survival only), plus air bubbles
// while underwater. Returns the y above everything it drew.
int drawStatusBars(TEXTURE &dst, bool underwater);

void hotbarPrev();
void hotbarNext();

#endif // HOTBAR_MODULE_H
