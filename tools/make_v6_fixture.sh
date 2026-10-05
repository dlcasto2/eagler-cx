#!/bin/sh
# Build Crafti v1.3 unchanged in a throwaway clone, run it headless to write
# one version-6 save with a known world and inventory, and copy that save to
# tests/fixtures/. The fixture is produced by v1.3's own Task::save.
set -eu

REPO="$(git rev-parse --show-toplevel)"
WORK=/tmp/crafti-v13-fixture
V13=b71390f

rm -rf "$WORK"
git clone -q "$REPO" "$WORK"
git -C "$WORK" checkout -q "$V13"
git -C "$WORK" submodule update --init -q

python3 - "$WORK/main.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace('#include "worldtask.h"', '#include "worldtask.h"\n#include "inventory.h"', 1)
hook = '''        Task::current_task->logic();
        {
            static int fixture_frame = 0;
            if(++fixture_frame == 3)
            {
                world.changeBlock(3, 30, 3, BLOCK_DIAMOND);
                world.changeBlock(-5, 20, 7, BLOCK_GLASS);
                const BLOCK_WDATA fx[] = { BLOCK_GOLD, BLOCK_TNT, BLOCK_GLASS, BLOCK_BOOKSHELF, BLOCK_PUMPKIN };
                for(unsigned i = 0; i < 5; ++i) current_inventory.entries[i] = fx[i];
                current_inventory.current_slot = 2;
                Task::save();
                Task::running = false;
            }
        }'''
s = s.replace("        Task::current_task->logic();", hook, 1)
open(p, "w").write(s)
PY

make -C "$WORK" -f Makefile.pc -j"$(nproc)"
rm -f /tmp/v6_fixture.map.tns
SDL_VIDEODRIVER=dummy "$WORK/crafti.elf" /tmp/v6_fixture.map.tns
cp /tmp/v6_fixture.map.tns "$REPO/tests/fixtures/v6_world.map.tns"
rm -rf "$WORK"
echo "fixture written: tests/fixtures/v6_world.map.tns"
