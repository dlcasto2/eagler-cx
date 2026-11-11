#ifndef MONO_ART_H
#define MONO_ART_H

// Converts the hand-drawn UI art (HUD icons, item icons, title wordmark, sun
// and moon) to greyscale on the classic (4bpp) TI-Nspire, matching what
// terrainInit() already does for the terrain atlas and main() for the
// loading screen. Call once, after nglInit() and before anything is drawn.
// A no-op on the TI-Nspire CX and on the PC build.
void monoArtInit();

#endif // MONO_ART_H
