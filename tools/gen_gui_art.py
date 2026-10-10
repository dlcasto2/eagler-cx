#!/usr/bin/env python3
"""Draws Crafti Survival Edition's GUI art and writes gui_art.cpp.

Every pixel here is original: the font, hearts, hunger, air bubbles,
crack stages and the title wordmark are drawn below as text grids or
small procedures. Nothing is taken from Minecraft or Eaglercraft.

Usage: python3 tools/gen_gui_art.py   (run from the repository root)
Writes gui_art.cpp and PNG previews in tools/preview/.
"""
import os
import random

try:
    from PIL import Image
except ImportError:  # previews are optional
    Image = None

KEY = 0xF81F  # transparent key colour (magenta), RGB565

# ---------------------------------------------------------------- font
# 5 columns wide, 9 rows: rows 0-6 are cap height, 7-8 are descenders.
# Glyph widths are trimmed automatically; space is 3 wide.
GLYPHS = {
' ': [],
'!': ['#', '#', '#', '#', '#', '.', '#'],
'"': ['#.#', '#.#'],
'#': ['.#.#.', '#####', '.#.#.', '.#.#.', '#####', '.#.#.'],
'$': ['..#..', '.####', '#.#..', '.###.', '..#.#', '####.', '..#..'],
'%': ['##..#', '##.#.', '..#..', '.#...', '#..##', '...##'],
'&': ['.##..', '#..#.', '.##..', '#..#.', '#...#', '.###.'],
"'": ['#', '#'],
'(': ['..#', '.#.', '#..', '#..', '#..', '.#.', '..#'],
')': ['#..', '.#.', '..#', '..#', '..#', '.#.', '#..'],
'*': ['', '#.#', '.#.', '#.#'],
'+': ['', '..#..', '..#..', '#####', '..#..', '..#..'],
',': ['', '', '', '', '', '.#', '.#', '#.'],
'-': ['', '', '', '####'],
'.': ['', '', '', '', '', '', '#'],
'/': ['....#', '...#.', '...#.', '..#..', '.#...', '.#...', '#....'],
'0': ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
'1': ['.#.', '##.', '.#.', '.#.', '.#.', '.#.', '###'],
'2': ['.###.', '#...#', '....#', '..##.', '.#...', '#....', '#####'],
'3': ['.###.', '#...#', '....#', '..##.', '....#', '#...#', '.###.'],
'4': ['...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.'],
'5': ['#####', '#....', '####.', '....#', '....#', '#...#', '.###.'],
'6': ['..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.'],
'7': ['#####', '....#', '...#.', '..#..', '..#..', '..#..', '..#..'],
'8': ['.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.'],
'9': ['.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..'],
':': ['', '', '#', '', '', '', '#'],
';': ['', '', '.#', '', '', '.#', '.#', '#.'],
'<': ['...#', '..#.', '.#..', '#...', '.#..', '..#.', '...#'],
'=': ['', '', '####', '', '####'],
'>': ['#...', '.#..', '..#.', '...#', '..#.', '.#..', '#...'],
'?': ['.###.', '#...#', '....#', '...#.', '..#..', '', '..#..'],
'@': ['.###.', '#...#', '#.###', '#.#.#', '#.###', '#....', '.###.'],
'A': ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
'B': ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.'],
'C': ['.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.'],
'D': ['###..', '#..#.', '#...#', '#...#', '#...#', '#..#.', '###..'],
'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
'F': ['#####', '#....', '#....', '####.', '#....', '#....', '#....'],
'G': ['.####', '#....', '#....', '#..##', '#...#', '#...#', '.###.'],
'H': ['#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
'I': ['###', '.#.', '.#.', '.#.', '.#.', '.#.', '###'],
'J': ['....#', '....#', '....#', '....#', '#...#', '#...#', '.###.'],
'K': ['#...#', '#..#.', '#.#..', '##...', '#.#..', '#..#.', '#...#'],
'L': ['#....', '#....', '#....', '#....', '#....', '#....', '#####'],
'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
'N': ['#...#', '##..#', '#.#.#', '#..##', '#...#', '#...#', '#...#'],
'O': ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
'P': ['####.', '#...#', '#...#', '####.', '#....', '#....', '#....'],
'Q': ['.###.', '#...#', '#...#', '#...#', '#.#.#', '#..#.', '.##.#'],
'R': ['####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'],
'S': ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
'T': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'],
'U': ['#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
'V': ['#...#', '#...#', '#...#', '#...#', '.#.#.', '.#.#.', '..#..'],
'W': ['#...#', '#...#', '#...#', '#.#.#', '#.#.#', '##.##', '#...#'],
'X': ['#...#', '.#.#.', '..#..', '..#..', '..#..', '.#.#.', '#...#'],
'Y': ['#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..', '..#..'],
'Z': ['#####', '....#', '...#.', '..#..', '.#...', '#....', '#####'],
'[': ['###', '#..', '#..', '#..', '#..', '#..', '###'],
'\\': ['#....', '.#...', '.#...', '..#..', '...#.', '...#.', '....#'],
']': ['###', '..#', '..#', '..#', '..#', '..#', '###'],
'^': ['.#.', '#.#'],
'_': ['', '', '', '', '', '', '', '#####'],
'`': ['#.', '.#'],
'a': ['', '', '.###.', '....#', '.####', '#...#', '.####'],
'b': ['#....', '#....', '#.##.', '##..#', '#...#', '#...#', '####.'],
'c': ['', '', '.###.', '#....', '#....', '#....', '.###.'],
'd': ['....#', '....#', '.##.#', '#..##', '#...#', '#...#', '.####'],
'e': ['', '', '.###.', '#...#', '#####', '#....', '.###.'],
'f': ['..##', '.#..', '####', '.#..', '.#..', '.#..', '.#..'],
'g': ['', '', '.####', '#...#', '#...#', '.####', '....#', '....#', '####.'],
'h': ['#....', '#....', '#.##.', '##..#', '#...#', '#...#', '#...#'],
'i': ['#', '', '#', '#', '#', '#', '#'],
'j': ['...#', '', '...#', '...#', '...#', '...#', '#..#', '#..#', '.##.'],
'k': ['#...', '#...', '#..#', '#.#.', '##..', '#.#.', '#..#'],
'l': ['#.', '#.', '#.', '#.', '#.', '#.', '.#'],
'm': ['', '', '##.#.', '#.#.#', '#.#.#', '#...#', '#...#'],
'n': ['', '', '####.', '#...#', '#...#', '#...#', '#...#'],
'o': ['', '', '.###.', '#...#', '#...#', '#...#', '.###.'],
'p': ['', '', '#.##.', '##..#', '#...#', '####.', '#....', '#....', '#....'],
'q': ['', '', '.##.#', '#..##', '#...#', '.####', '....#', '....#', '....#'],
'r': ['', '', '#.##.', '##..#', '#....', '#....', '#....'],
's': ['', '', '.####', '#....', '.###.', '....#', '####.'],
't': ['.#..', '.#..', '####', '.#..', '.#..', '.#..', '..##'],
'u': ['', '', '#...#', '#...#', '#...#', '#...#', '.####'],
'v': ['', '', '#...#', '#...#', '#...#', '.#.#.', '..#..'],
'w': ['', '', '#...#', '#...#', '#.#.#', '#.#.#', '.#.#.'],
'x': ['', '', '#...#', '.#.#.', '..#..', '.#.#.', '#...#'],
'y': ['', '', '#...#', '#...#', '#...#', '.####', '....#', '....#', '####.'],
'z': ['', '', '#####', '...#.', '..#..', '.#...', '#####'],
'{': ['..##', '.#..', '.#..', '#...', '.#..', '.#..', '..##'],
'|': ['#', '#', '#', '#', '#', '#', '#', '#'],
'}': ['##..', '..#.', '..#.', '...#', '..#.', '..#.', '##..'],
'~': ['', '', '', '.#..#', '#.##.'],
}

FONT_ROWS = 9


def glyph_bits(ch):
    rows = list(GLYPHS[ch]) + [''] * FONT_ROWS
    rows = rows[:FONT_ROWS]
    cols = max([len(r) for r in rows] + [0])
    if ch == ' ':
        return 3, [0] * FONT_ROWS
    # trim empty columns on the left and right
    used = [c for c in range(cols) if any(c < len(r) and r[c] == '#' for r in rows)]
    lo, hi = min(used), max(used)
    width = hi - lo + 1
    bits = []
    for r in rows:
        v = 0
        for c in range(lo, hi + 1):
            if c < len(r) and r[c] == '#':
                v |= 1 << (4 - (c - lo))
        bits.append(v)
    return width, bits


# --------------------------------------------------------- tiny images
class Img:
    """A w x h canvas of RGB tuples or None (transparent)."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[None] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def get(self, x, y):
        return self.px[y][x]

    def blit(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                c = other.px[y][x]
                if c is not None:
                    self.set(ox + x, oy + y, c)

    def to565(self, key=KEY):
        out = []
        for row in self.px:
            for c in row:
                if c is None:
                    out.append(key)
                else:
                    r, g, b = c
                    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    if v == key:
                        v = key ^ 0x20  # never collide with the key
                    out.append(v)
        return out

    def save_preview(self, path, scale=4):
        if Image is None:
            return
        im = Image.new('RGBA', (self.w, self.h), (0, 0, 0, 0))
        for y in range(self.h):
            for x in range(self.w):
                c = self.px[y][x]
                if c is not None:
                    im.putpixel((x, y), c + (255,))
        bg = Image.new('RGBA', im.size, (60, 60, 70, 255))
        bg.alpha_composite(im)
        bg.resize((self.w * scale, self.h * scale), Image.NEAREST).save(path)


def from_grid(rows, palette):
    img = Img(len(rows[0]), len(rows))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch != '.':
                img.set(x, y, palette[ch])
    return img


# ------------------------------------------------- HUD icons (9 x 9)
HEART = [
    '.##...##.',
    '#rr#.#rr#',
    '#rwrrrrr#',
    '#rwrrrrd#',
    '.#rrrrd#.',
    '..#rrd#..',
    '...#d#...',
    '....#....',
    '.........',
]
HEART_PAL = {'#': (24, 8, 8), 'r': (214, 36, 36), 'w': (255, 196, 196), 'd': (140, 18, 18)}
HEART_EMPTY_PAL = {'#': (24, 8, 8), 'r': (52, 30, 30), 'w': (70, 46, 46), 'd': (44, 24, 24)}

# hunger: a round loaf with a crust line (original design)
LOAF = [
    '.........',
    '..#####..',
    '.#cccch#.',
    '#cclcccc#',
    '#cccclcc#',
    '#bccccbb#',
    '.#bbbbb#.',
    '..#####..',
    '.........',
]
LOAF_PAL = {'#': (36, 20, 6), 'c': (206, 140, 62), 'h': (250, 214, 150), 'l': (150, 92, 34), 'b': (150, 92, 34)}
LOAF_EMPTY_PAL = {'#': (36, 20, 6), 'c': (58, 44, 30), 'h': (74, 58, 42), 'l': (48, 36, 24), 'b': (48, 36, 24)}

BUBBLE = [
    '.........',
    '...###...',
    '..#wbb#..',
    '.#wbbbb#.',
    '.#bbbbb#.',
    '.#bbbbd#.',
    '..#bdd#..',
    '...###...',
    '.........',
]
BUBBLE_PAL = {'#': (20, 40, 110), 'w': (240, 250, 255), 'b': (110, 170, 240), 'd': (60, 110, 200)}
BUBBLE_POP = [
    '.........',
    '..#...#..',
    '...#.#...',
    '.#.....#.',
    '.........',
    '.#.....#.',
    '...#.#...',
    '..#...#..',
    '.........',
]


def half_of(full, empty):
    """Left half full, right half empty."""
    img = Img(9, 9)
    for y in range(9):
        for x in range(9):
            img.set(x, y, full.get(x, y) if x < 5 else empty.get(x, y))
    return img


def hud_strip():
    """[heart full, half, empty, loaf full, half, empty, bubble, pop] as 9x9 cells."""
    hf, he = from_grid(HEART, HEART_PAL), from_grid(HEART, HEART_EMPTY_PAL)
    lf, le = from_grid(LOAF, LOAF_PAL), from_grid(LOAF, LOAF_EMPTY_PAL)
    # the hunger bar fills from the right in 1.8.8, so its half is the right half
    lh = Img(9, 9)
    for y in range(9):
        for x in range(9):
            lh.set(x, y, lf.get(x, y) if x >= 4 else le.get(x, y))
    cells = [hf, half_of(hf, he), he, lf, lh, le,
             from_grid(BUBBLE, BUBBLE_PAL), from_grid(BUBBLE_POP, {'#': (180, 210, 255)})]
    strip = Img(9 * len(cells), 9)
    for i, c in enumerate(cells):
        strip.blit(c, 9 * i, 0)
    return strip


# ------------------------------------------- crack stages (10 x 16x16)
def crack_strip():
    rnd = random.Random(1885)
    strip = Img(160, 16)
    lines = []
    # each stage adds two short random-walk cracks from the centre outward
    for stage in range(10):
        for _ in range(2):
            x, y = 7.5 + rnd.uniform(-3, 3), 7.5 + rnd.uniform(-3, 3)
            dx, dy = rnd.choice([(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)])
            pts = []
            for _ in range(3 + stage):
                pts.append((int(x), int(y)))
                if rnd.random() < 0.35:
                    dx, dy = rnd.choice([(dx, 0), (0, dy), (dx, dy)]) if (dx or dy) else (1, 0)
                    if dx == 0 and dy == 0:
                        dx = 1
                x += dx
                y += dy
            lines.append(pts)
        for pts in lines:
            for (x, y) in pts:
                if 0 <= x < 16 and 0 <= y < 16:
                    strip.set(stage * 16 + x, y, (20, 20, 20))
    return strip


# ------------------------------------------------- title wordmark
def title_logo(text='CRAFTI', scale=4):
    rnd = random.Random(42)
    glyphs = [glyph_bits(c) for c in text]
    w = sum((g[0] + 1) * scale for g in glyphs) + 4
    h = 7 * scale + 5
    img = Img(w, h)
    shadow = Img(w, h)
    x0 = 1
    for width, bits in glyphs:
        for gy in range(7):
            for gx in range(width):
                if bits[gy] & (1 << (4 - gx)):
                    for sy in range(scale):
                        for sx in range(scale):
                            px, py = x0 + gx * scale + sx, 1 + gy * scale + sy
                            # speckled stone fill, lighter toward the top
                            base = 205 - py * 3 + rnd.randint(-22, 22)
                            base = max(70, min(245, base))
                            img.set(px, py, (base, base, base))
                            for d in (1, 2, 3):
                                shadow.set(px + d, py + d, (34, 34, 34) if d < 3 else (10, 10, 10))
        x0 += (width + 1) * scale
    out = Img(w, h)
    out.blit(shadow, 0, 0)
    out.blit(img, 0, 0)
    # one-pixel dark outline so the letters read on any background
    final = Img(w, h)
    for y in range(h):
        for x in range(w):
            if out.get(x, y) is None and any(
                    0 <= x + dx < w and 0 <= y + dy < h and img.get(x + dx, y + dy) is not None
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                final.set(x, y, (10, 10, 10))
    final.blit(out, 0, 0)
    return final



# ------------------------------------------- sun and moon (32 x 32)
def sun_disc():
    """A round sun: white-hot middle, yellow body, orange rim, soft corona."""
    img = Img(32, 32)
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5
            r = (dx * dx + dy * dy) ** 0.5
            if r < 7:
                img.set(x, y, (255, 252, 226))
            elif r < 10:
                img.set(x, y, (255, 238, 120))
            elif r < 12:
                img.set(x, y, (255, 206, 64))
            elif r < 13:
                img.set(x, y, (250, 160, 40))
            elif r < 15.5 and (x + y) % 2 == 0:
                img.set(x, y, (255, 226, 140))   # dithered glow
    return img


def moon_disc():
    """A pale moon with a few soft grey craters."""
    img = Img(32, 32)
    craters = [(11, 12, 3), (19, 18, 2.5), (14, 21, 1.8), (20, 10, 1.6)]
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5
            r = (dx * dx + dy * dy) ** 0.5
            if r < 11:
                c = (226, 230, 240)
                for cx, cy, cr in craters:
                    if (x - cx) ** 2 + (y - cy) ** 2 < cr * cr:
                        c = (176, 182, 198)
                if x - y > 8:
                    c = (c[0] - 30, c[1] - 30, c[2] - 20)   # shaded edge
                img.set(x, y, c)
            elif r < 12:
                img.set(x, y, (150, 156, 176))
    return img

# ------------------------------------------------------------ output
def c_array(name, data, per_line=16):
    lines = ['static COLOR %s_data[] = {' % name]
    for i in range(0, len(data), per_line):
        lines.append('    ' + ', '.join('0x%04X' % v for v in data[i:i + per_line]) + ',')
    lines.append('};')
    return '\n'.join(lines)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    preview = os.path.join(root, 'tools', 'preview')
    os.makedirs(preview, exist_ok=True)

    images = {
        'gui_hud': hud_strip(),
        'gui_crack': crack_strip(),
        'gui_title': title_logo(),
    }

    out = ['// Generated by tools/gen_gui_art.py - do not edit by hand.',
           '// All art is original to Crafti Survival Edition.',
           '#include "gui_art.h"', '']

    widths, rows = [], []
    for code in range(32, 127):
        wdt, bits = glyph_bits(chr(code))
        widths.append(wdt)
        rows.append(bits)
    out.append('const uint8_t gui_font_widths[95] = {' + ', '.join(map(str, widths)) + '};')
    out.append('const uint8_t gui_font_rows[95][%d] = {' % FONT_ROWS)
    for code, bits in zip(range(32, 127), rows):
        label = chr(code) if chr(code) not in '\\' else 'backslash'
        out.append('    {' + ', '.join('0x%02X' % b for b in bits) + '}, // ' + label)
    out.append('};')
    out.append('')

    for name, img in images.items():
        out.append(c_array(name, img.to565()))
        out.append('TEXTURE %s = { %d, %d, true, 0x%04X, %s_data };' % (name, img.w, img.h, KEY, name))
        out.append('')
        img.save_preview(os.path.join(preview, name + '.png'))

    # Drawn in the 3D sky by nGL, which treats black (0x0000) as transparent.
    for name, img in (('gui_sun', sun_disc()), ('gui_moon', moon_disc())):
        out.append(c_array(name, img.to565(key=0x0000)))
        out.append('TEXTURE %s = { %d, %d, true, 0x0000, %s_data };' % (name, img.w, img.h, name))
        out.append('')
        img.save_preview(os.path.join(preview, name + '.png'))

    # font preview
    sample = ['ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz', '0123456789 !?.,:;+-=/()[]<>%#"\'']
    fw = max(sum(glyph_bits(c)[0] + 1 for c in s) for s in sample) + 2
    font_img = Img(fw, 11 * len(sample) + 2)
    for li, s in enumerate(sample):
        x = 1
        for ch in s:
            wdt, bits = glyph_bits(ch)
            for y in range(FONT_ROWS):
                for gx in range(wdt):
                    if bits[y] & (1 << (4 - gx)):
                        font_img.set(x + gx + 1, 1 + li * 11 + y + 1, (62, 62, 62))
                        font_img.set(x + gx, 1 + li * 11 + y, (255, 255, 255))
            x += wdt + 1
    font_img.save_preview(os.path.join(preview, 'gui_font.png'))

    with open(os.path.join(root, 'gui_art.cpp'), 'w') as f:
        f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
