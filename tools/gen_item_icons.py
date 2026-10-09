#!/usr/bin/env python3
"""Draws the 16x16 item icons for items 256-344 and writes item_icons.cpp.

All icons are original: each is a hand-drawn shape template below,
recoloured per material. Nothing is taken from Minecraft or Eaglercraft.

Usage: python3 tools/gen_item_icons.py   (run from the repository root)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_gui_art import Img, KEY, c_array  # noqa: E402

# Template letters: o outline, m main, h highlight, s shadow,
# w handle, d handle shadow, x accent. '.' is transparent.
T = {}
T['pickaxe'] = [
    '................',
    '....oooooooo....',
    '...ohhhmmmmmo...',
    '..ommoooooosmo..',
    '..oso....ows.o..',
    '...o....owd..o..',
    '.......owd......',
    '......owd.......',
    '.....owd........',
    '....owd.........',
    '...owd..........',
    '..owd...........',
    '.owd............',
    '.od.............',
    '..o.............',
    '................',
]
T['axe'] = [
    '................',
    '.......ooo......',
    '......ohhmo.....',
    '.....ohmmmso....',
    '.....omoowmso...',
    '......o.owdso...',
    '.......owd.o....',
    '......owd.......',
    '.....owd........',
    '....owd.........',
    '...owd..........',
    '..owd...........',
    '.owd............',
    '.od.............',
    '..o.............',
    '................',
]
T['shovel'] = [
    '................',
    '..........ooo...',
    '.........ohmmo..',
    '.........ohmmso.',
    '........ommmsso.',
    '.........omsso..',
    '........owdoo...',
    '.......owd......',
    '......owd.......',
    '.....owd........',
    '....owd.........',
    '...owd..........',
    '..owd...........',
    '.owd............',
    '.oo.............',
    '................',
]
T['sword'] = [
    '................',
    '............ooo.',
    '...........ohmo.',
    '..........ohmso.',
    '.........ohmso..',
    '........ohmso...',
    '.......ohmso....',
    '..oo..ohmso.....',
    '..oxoohmso......',
    '...oxxmso.......',
    '....oxxo........',
    '...owdoxo.......',
    '..owdo.oxo......',
    '.oodo...oo......',
    '.ooo............',
    '................',
]
T['bow'] = [
    '................',
    '.......oooo.....',
    '.....oowwwdo....',
    '....owdoooo.x...',
    '...owdo....x....',
    '..owdo....x.....',
    '..owo....x......',
    '.owdo...x.......',
    '.owo...x........',
    '.owo..x.........',
    '.owo.x..........',
    '.owdx...........',
    '..owo...........',
    '..owdo..........',
    '...oo...........',
    '................',
]
T['stick'] = [
    '................',
    '................',
    '............oo..',
    '...........owdo.',
    '..........owdo..',
    '.........owdo...',
    '........owdo....',
    '.......owdo.....',
    '......owdo......',
    '.....owdo.......',
    '....owdo........',
    '...owdo.........',
    '..owdo..........',
    '..odo...........',
    '...o............',
    '................',
]
T['ingot'] = [
    '................',
    '................',
    '................',
    '................',
    '.......oooooo...',
    '.....oohhhhmmo..',
    '...oohhmmmmmsso.',
    '..ohmmmmmmmssso.',
    '.ohhmmmmmmsssoo.',
    '.ommmmmmssssoo..',
    '.osssssssoo.....',
    '..oooooooo......',
    '................',
    '................',
    '................',
    '................',
]
T['gem'] = [
    '................',
    '................',
    '.....oooooo.....',
    '....ohhhmmmo....',
    '...ohhmmmmmso...',
    '..ohmmmhmmmsso..',
    '..ommmhmmmmsso..',
    '..ommmmmmmssso..',
    '...ommmmmsssoo..',
    '....ommmssso....',
    '.....omssso.....',
    '......osso......',
    '.......oo.......',
    '................',
    '................',
    '................',
]
T['crystal'] = [
    '................',
    '.......oo.......',
    '......ohmo......',
    '.....ohmmso.....',
    '.....ohmmso.....',
    '....ohhmmsso....',
    '....ohmmmsso....',
    '....ohmmmsso....',
    '....ohmmmsso....',
    '....ohmmmsso....',
    '.....ommsso.....',
    '.....ommsso.....',
    '......omso......',
    '.......oo.......',
    '................',
    '................',
]
T['lump'] = [
    '................',
    '................',
    '................',
    '......ooooo.....',
    '....oohhmmmoo...',
    '...ohhmmmmmmso..',
    '..ohmmmxmmmmso..',
    '..ommmmmmmxmsso.',
    '..ommxmmmmmmsso.',
    '..ommmmmmmmssso.',
    '...osmmmxmsssoo.',
    '....oosssssoo...',
    '......ooooo.....',
    '................',
    '................',
    '................',
]
T['dust'] = [
    '................',
    '................',
    '................',
    '................',
    '........h.......',
    '.....h.m....h...',
    '.......mm.m.....',
    '...m..mhms..m...',
    '.....mhmmsm.....',
    '..m.mhmmmmsm.m..',
    '...mhmmmmmmsm...',
    '..mmmmmmmmmmss..',
    '.ssssssssssssss.',
    '................',
    '................',
    '................',
]
T['scrap'] = [
    '................',
    '................',
    '................',
    '.....ooo........',
    '....ohmxoo......',
    '...ohmmmmxoo....',
    '...ommxmmmmso...',
    '..ohmmmmmxmmso..',
    '..ommxmmmmmsso..',
    '...osmmmmxmsso..',
    '....ossmmmsso...',
    '.....oossssoo...',
    '.......oooo.....',
    '................',
    '................',
    '................',
]
T['bone'] = [
    '................',
    '..oo............',
    '.ohho...........',
    '.ohmmo..........',
    '..ommmo.........',
    '...ommso........',
    '....ommso.......',
    '.....ommso......',
    '......ommso.....',
    '.......ommso....',
    '........ommso...',
    '.........ommso..',
    '..........ommso.',
    '...........osso.',
    '............oo..',
    '................',
]
T['arrow'] = [
    '................',
    '...........ooo..',
    '..........ohmo..',
    '..........omso..',
    '.........owsoo..',
    '........owdo....',
    '.......owdo.....',
    '......owdo......',
    '.....owdo.......',
    '....owdo........',
    '..xxwdo.........',
    '.xxxdo..........',
    '.xxxx...........',
    '..xx............',
    '................',
    '................',
]
T['hide'] = [
    '................',
    '................',
    '...oo......oo...',
    '..ohmooooooomo..',
    '..ommhhmmmmmso..',
    '...ommmmxmmmso..',
    '...ohmmmmmmsso..',
    '...ommmxmmmso...',
    '...ommmmmmmso...',
    '..ohmmmmmxmsso..',
    '..ommmmmmmmsso..',
    '..osmooooomsso..',
    '...oo.....oso...',
    '...........o....',
    '................',
    '................',
]
T['feather'] = [
    '................',
    '...........oo...',
    '..........ohmo..',
    '.........ohhmo..',
    '........ohhmmo..',
    '.......ohhmmso..',
    '......ohhmmso...',
    '.....ohhmmso....',
    '....ohhmmso.....',
    '....ohmmso......',
    '...odmsoo.......',
    '..odoo..........',
    '.odo............',
    '.oo.............',
    '................',
    '................',
]
T['string'] = [
    '................',
    '................',
    '...........hh...',
    '..........h..m..',
    '..........m..m..',
    '...........mm...',
    '..........m.....',
    '.........m......',
    '........m.......',
    '.......m........',
    '......m.........',
    '.....m..........',
    '..mm.m..........',
    '.m..m...........',
    '..mm............',
    '................',
]
T['wheat'] = [
    '................',
    '..........o.....',
    '.......o.oho.o..',
    '......ohoomoohoo',
    '.......omoohomo.',
    '....o..oomomoo..',
    '...ohoohoomoo...',
    '....omoomxoo....',
    '...ohoomxoo.....',
    '....oomxo.......',
    '.....oxo........',
    '....oxo.........',
    '...oxo..........',
    '..oxo...........',
    '..oo............',
    '................',
]
T['apple'] = [
    '................',
    '........wo......',
    '.......owxx.....',
    '.....oowoxxo....',
    '...oommmwmmoo...',
    '..ohhmmmmmmmso..',
    '..ohmmmmmmmmso..',
    '.ohmmmmmmmmmsso.',
    '.ohmmmmmmmmmsso.',
    '.ommmmmmmmmssso.',
    '.ommmmmmmmmssso.',
    '..ommmmmmmssso..',
    '..osmmmmmssso...',
    '...oossoosoo....',
    '.....oo..oo.....',
    '................',
]
T['bread'] = [
    '................',
    '................',
    '................',
    '................',
    '.......ooooo....',
    '....ooohhhmmoo..',
    '..oohhmmxmmmmso.',
    '.ohhmmxmmmxmmso.',
    '.ohmmmmmxmmmsso.',
    '.ommmxmmmmmssso.',
    '.osmmmmmmssssoo.',
    '..ossssssssoo...',
    '...ooooooooo....',
    '................',
    '................',
    '................',
]
T['steak'] = [
    '................',
    '................',
    '.....ooooo......',
    '...oohhmmmoo....',
    '..ohhmmmmmmmo...',
    '.ohmmmxxmmmmso..',
    '.ommmxwwxmmmso..',
    '.ommmxwwxmmmsso.',
    '.ommmmxxmmmmsso.',
    '..ommmmmmmmmsso.',
    '..osmmmmmmmssoo.',
    '...ossmmmssso...',
    '....oooosssoo...',
    '........ooo.....',
    '................',
    '................',
]
T['drumstick'] = [
    '................',
    '................',
    '.....ooooo......',
    '...oohhmmmoo....',
    '..ohhmmmmmmmo...',
    '..ohmmmmmmmmso..',
    '..ommmmmmmmmso..',
    '..ommmmmmmmsso..',
    '...ommmmmmsso...',
    '....oommssoo....',
    '......owwo......',
    '......owwo......',
    '.....owwwwo.....',
    '.....owddwo.....',
    '......oooo......',
    '................',
]
T['chop'] = [
    '................',
    '................',
    '................',
    '.....ooooooo....',
    '...oohhmmmmmoo..',
    '..ohhmmmmmmmmso.',
    '..ohmmxxxxmmmso.',
    '..ommmmmmmmmsso.',
    '..ommmmmmmmssso.',
    '...osmmmmmsssoo.',
    '....oossssoo....',
    '......owwo......',
    '.....owwwwo.....',
    '.....owddwo.....',
    '......oooo......',
    '................',
]

# Colour sets: (outline, main, highlight, shadow)
def pal(o, m, h, s, w=(150, 104, 52), d=(96, 64, 30), x=None):
    return {'o': o, 'm': m, 'h': h, 's': s, 'w': w, 'd': d, 'x': x if x else s}

BLK = (16, 16, 16)
MATERIAL = {
    'wood': pal(BLK, (160, 118, 64), (204, 160, 98), (110, 78, 40)),
    'stone': pal(BLK, (130, 130, 130), (176, 176, 176), (88, 88, 88)),
    'iron': pal(BLK, (206, 206, 206), (250, 250, 250), (140, 140, 140)),
    'gold': pal(BLK, (236, 196, 40), (255, 244, 140), (176, 128, 20)),
    'diamond': pal(BLK, (72, 220, 210), (200, 255, 250), (28, 150, 150)),
    'netherite': pal(BLK, (76, 66, 74), (122, 110, 118), (44, 38, 44)),
}

SPEC = {}  # item id -> (template, palette)
FIRST_TOOL = 320
KINDS = ['pickaxe', 'axe', 'shovel', 'sword']
for mi, mat in enumerate(['wood', 'stone', 'iron', 'gold', 'diamond', 'netherite']):
    for ki, kind in enumerate(KINDS):
        p = dict(MATERIAL[mat])
        if kind == 'sword':
            p['x'] = (90, 60, 30)
        SPEC[FIRST_TOOL + 4 * mi + ki] = (kind, p)
SPEC[344] = ('bow', pal(BLK, BLK, BLK, BLK, x=(230, 230, 230)))

SPEC.update({
    256: ('stick', pal(BLK, BLK, BLK, BLK)),
    257: ('lump', pal(BLK, (44, 44, 48), (90, 90, 96), (24, 24, 26), x=(20, 20, 22))),
    258: ('lump', pal(BLK, (62, 50, 40), (110, 92, 74), (36, 28, 22), x=(26, 20, 16))),
    259: ('ingot', MATERIAL['iron']),
    260: ('ingot', MATERIAL['gold']),
    261: ('gem', MATERIAL['diamond']),
    262: ('crystal', pal(BLK, (40, 196, 90), (170, 255, 190), (16, 120, 50))),
    263: ('dust', pal(BLK, (210, 20, 20), (255, 120, 110), (120, 10, 10))),
    264: ('scrap', pal(BLK, (92, 70, 62), (140, 116, 104), (54, 40, 36), x=(160, 130, 60))),
    265: ('ingot', MATERIAL['netherite']),
    266: ('bone', pal((70, 66, 56), (226, 222, 204), (255, 255, 244), (170, 164, 144))),
    267: ('arrow', pal(BLK, (150, 150, 150), (210, 210, 210), (90, 90, 90), x=(238, 238, 238))),
    268: ('dust', pal(BLK, (88, 88, 88), (150, 150, 150), (50, 50, 50))),
    269: ('hide', pal((48, 26, 12), (156, 86, 42), (200, 128, 70), (104, 54, 24), x=(124, 66, 30))),
    270: ('feather', pal((90, 90, 96), (232, 232, 236), (255, 255, 255), (178, 178, 186), d=(120, 120, 128))),
    271: ('string', pal(BLK, (236, 236, 236), (255, 255, 255), (200, 200, 200))),
    272: ('wheat', pal((90, 70, 20), (220, 186, 70), (250, 226, 130), (170, 136, 40), x=(150, 140, 40))),
    288: ('apple', pal((60, 10, 10), (214, 30, 36), (255, 140, 130), (140, 14, 20), w=(110, 74, 34), x=(60, 160, 40))),
    289: ('bread', pal((60, 34, 10), (198, 136, 60), (240, 196, 120), (140, 86, 32), x=(160, 104, 44))),
    290: ('chop', pal((70, 20, 30), (236, 140, 150), (255, 200, 204), (190, 90, 104), x=(255, 236, 236))),
    291: ('chop', pal((50, 24, 10), (196, 120, 70), (236, 176, 120), (140, 76, 40), x=(250, 220, 170))),
    292: ('steak', pal((70, 10, 14), (204, 44, 50), (250, 120, 120), (140, 20, 26), w=(250, 236, 236), x=(250, 200, 200))),
    293: ('steak', pal((40, 18, 6), (130, 72, 34), (186, 120, 70), (86, 44, 18), w=(220, 180, 140), x=(170, 110, 60))),
    294: ('drumstick', pal((80, 50, 40), (246, 200, 186), (255, 236, 228), (210, 150, 140), w=(240, 236, 220), d=(190, 180, 160))),
    295: ('drumstick', pal((50, 26, 8), (210, 140, 66), (246, 196, 120), (150, 90, 34), w=(240, 236, 220), d=(190, 180, 160))),
    296: ('steak', pal((70, 14, 20), (214, 64, 70), (250, 140, 140), (150, 30, 40), w=(250, 250, 250), x=(255, 220, 220))),
    297: ('steak', pal((44, 20, 10), (150, 88, 52), (200, 140, 96), (100, 56, 30), w=(240, 220, 200), x=(190, 130, 90))),
    298: ('steak', pal((30, 40, 16), (130, 120, 60), (170, 160, 96), (90, 80, 34), w=(160, 170, 110), x=(110, 60, 50))),
})

FIRST_ITEM, LAST_ITEM = 256, 344
COLS = 16


def build_atlas():
    n = LAST_ITEM - FIRST_ITEM + 1
    rows = (n + COLS - 1) // COLS
    atlas = Img(COLS * 16, rows * 16)
    for item, (tname, p) in SPEC.items():
        grid = T[tname]
        assert len(grid) == 16 and all(len(r) == 16 for r in grid), tname
        i = item - FIRST_ITEM
        ox, oy = (i % COLS) * 16, (i // COLS) * 16
        for y, r in enumerate(grid):
            for x, ch in enumerate(r):
                if ch != '.':
                    atlas.set(ox + x, oy + y, p[ch])
    return atlas


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    atlas = build_atlas()
    os.makedirs(os.path.join(root, 'tools', 'preview'), exist_ok=True)
    atlas.save_preview(os.path.join(root, 'tools', 'preview', 'item_icons.png'))
    has = ['1' if (FIRST_ITEM + i) in SPEC else '0' for i in range(LAST_ITEM - FIRST_ITEM + 1)]
    out = ['// Generated by tools/gen_item_icons.py - do not edit by hand.',
           '// All icons are original to Crafti Survival Edition.',
           '#include "item_icons.h"', '',
           c_array('item_icon_atlas', atlas.to565()),
           'static TEXTURE atlas = { %d, %d, true, 0x%04X, item_icon_atlas_data };' % (atlas.w, atlas.h, KEY),
           'static const bool has_icon[%d] = {%s};' % (len(has), ', '.join('true' if h == '1' else 'false' for h in has)),
           '',
           'const TEXTURE &itemIconAtlas() { return atlas; }',
           '',
           'bool itemIconUV(ItemId id, int &u, int &v)',
           '{',
           '    if(id < %d || id > %d || !has_icon[id - %d])' % (FIRST_ITEM, LAST_ITEM, FIRST_ITEM),
           '        return false;',
           '    const int i = id - %d;' % FIRST_ITEM,
           '    u = (i %% %d) * 16;' % COLS,
           '    v = (i / %d) * 16;' % COLS,
           '    return true;',
           '}', '']
    with open(os.path.join(root, 'item_icons.cpp'), 'w') as f:
        f.write('\n'.join(out))


if __name__ == '__main__':
    main()
