#ifndef SAVEFILE_H
#define SAVEFILE_H

#include <string>
#include <zlib.h>

#include "gamestate.h"
#include "playerinventory.h"
#include "saveio.h"
#include "terrain.h"

enum class LoadResult { OK, MISSING, UNREADABLE };

// The version 7 survival section: state, inventory, block storage, item piles.
bool writeSurvivalSection(SaveWriter &w);
bool readSurvivalSection(SaveReader &r);

// Move a version 6 hotbar (5 blocks + selected slot) into the new inventory,
// in creative mode.
void convertV6Inventory(const BLOCK_WDATA (&entries)[5], int current_slot, PlayerInventory &inv, PlayerState &ps);

std::string backupPathFor(const std::string &path);
std::string tempPathFor(const std::string &path);
std::string newWorldPathFor(const std::string &path);

// Where to save given how the existing world loaded: a fresh path when the old
// save was unreadable (so it's never overwritten), otherwise the original.
std::string chooseSavePath(const std::string &original, LoadResult r);

// -1 missing, -2 unreadable, otherwise the leading version int.
int readSaveVersion(const char *path);

// Write via a temp file and swap in only on success, backing up a pre-v7 save
// once. Returns false without touching the old save if the writer fails.
bool replaceSaveSafely(const std::string &path, bool (*write)(gzFile, void *), void *ctx);

#endif // SAVEFILE_H
