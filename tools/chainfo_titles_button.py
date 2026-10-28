"""Draws the character window's titles button, Data/Interface/newui_chainfo_btn_titles.OZT (MU Moldova, titles over
the names, ghinzuro 2026-10-27).

Made as the advanced stats button was (scripts/chainfo-stats-button.py in the server repo): the quest, pet and stats
buttons are a forged stone square with a golden glyph, 36x29, the normal state above a greyed one (36x58 in all).
This takes the pet button, paints its eagle out with the stone of the quest and master level buttons, and draws a
golden title plate in the same manner: an outline, light from the upper left, a shadow to the lower right, a gradient
down the plate. The greyed half gets the same plate in the grey of the other buttons' greyed glyphs.

    python -I tools/chainfo_titles_button.py           write the file
    python -I tools/chainfo_titles_button.py --png X   also write a 4x preview of both states to X

An OZT is 4 bytes and a TGA: uncompressed 32-bit BGRA, rows bottom up, then the TGA 2.0 footer.
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, "src", "bin", "Data", "Interface")
SOURCE = os.path.join(DIR, "newui_chainfo_btn_pet.OZT")
TARGET = os.path.join(DIR, "newui_chainfo_btn_titles.OZT")
W, H, HALF = 36, 58, 29

# The plate, in a half's own coordinates (x right, y down): a ribbon with pointed ends, a line of "text" pressed into
# it, and a gem on its top edge, centred like the other buttons' glyphs.
PLATE_LEFT, PLATE_RIGHT, PLATE_TOP, PLATE_BOTTOM = 7, 29, 11, 19
TEXT_LINE = (13, 23, 15)                         # pressed line from x 13 to 23 at y 15
GEM = (18, 8, 3)                                 # centre x, y and radius of the diamond

# Colours taken from the book and eagle glyphs: a warm gold, darker and more orange towards the bottom.
GOLD = ((250, 214, 96), (176, 98, 16))         # top, bottom of the gradient
GOLD_EDGE = (46, 26, 4)
GREY = ((186, 178, 160), (104, 98, 86))
GREY_EDGE = (30, 28, 26)
OTHERS = ("quest", "master")                    # buttons whose stone fills the eagle's place where it is bare


def read(path):
    raw = open(path, "rb").read()
    tga = raw[4:]
    w, h, bpp = struct.unpack_from("<HHB", tga, 12)
    assert (w, h, bpp, tga[2]) == (W, H, 32, 2), "not a 36x58 uncompressed 32-bit button"
    start = 18 + tga[0]
    pixels = [list(tga[start + i * 4:start + i * 4 + 4]) for i in range(W * H)]   # BGRA
    return raw[:4], tga[:start], pixels, tga[start + W * H * 4:]


def at(pixels, x, y):
    """The pixel at (x, y) counted from the top of the image."""
    return pixels[(H - 1 - y) * W + x]


def glyph_mask(pixels, top, grow):
    """The glyph pixels of the half starting at row `top` (gold or grey is warmer than the stone), grown by
    `grow` pixels for its dark outline, inside the button's frame."""
    golden = set()
    for y in range(top, top + HALF):
        for x in range(W):
            b, g, r, a = at(pixels, x, y)
            if a > 128 and r - b > 10:
                golden.add((x, y))
    out = set()
    for x, y in golden:
        for dx in range(-grow, grow + 1):
            for dy in range(-grow, grow + 1):
                if 4 <= x + dx < W - 4 and top + 3 <= y + dy < top + HALF - 3:
                    out.add((x + dx, y + dy))
    return out


def paint_out_glyph(pixels, top, others):
    """Replaces the glyph of the half starting at row `top` with stone: from another button where that one is
    bare there, else the mean of the stone around it."""
    hole = glyph_mask(pixels, top, 2)
    for other in others:
        bare = glyph_mask(other, top, 2)
        for x, y in list(hole):
            if (x, y) not in bare:
                at(pixels, x, y)[:] = at(other, x, y)
                hole.discard((x, y))
    known = {(x, y) for y in range(top, top + HALF) for x in range(W)} - hole
    while hole:
        ring = []
        for x, y in hole:
            near = [at(pixels, x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if (x + dx, y + dy) in known]
            if near:
                ring.append(((x, y), [round(sum(p[c] for p in near) / len(near)) for c in range(3)]))
        for (x, y), colour in ring:
            at(pixels, x, y)[:3] = colour
            at(pixels, x, y)[3] = 255
            known.add((x, y))
            hole.discard((x, y))


def blend(c1, c2, t):
    return tuple(round(a + (b - a) * t) for a, b in zip(c1, c2))


def plate_shape():
    """The plate's pixels and its gem's, as {(x, y): t} with t the place in the gradient (0 top, 1 bottom)."""
    shape = {}
    middle = (PLATE_TOP + PLATE_BOTTOM) / 2
    for y in range(PLATE_TOP, PLATE_BOTTOM + 1):
        inset = round(abs(y - middle))
        for x in range(PLATE_LEFT + inset, PLATE_RIGHT - inset + 1):
            shape[(x, y)] = (y - PLATE_TOP) / (PLATE_BOTTOM - PLATE_TOP)
    cx, cy, r = GEM
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if abs(x - cx) + abs(y - cy) <= r:
                shape[(x, y)] = 0.15 + 0.5 * (y - (cy - r)) / (2 * r)
    return shape


def draw_chart(pixels, top, colours, edge):
    shape = plate_shape()
    filled = {}
    for (x, y), t in shape.items():
        colour = blend(colours[0], colours[1], max(0.0, min(1.0, t)))
        if (x - 1, y) not in shape or (x, y - 1) not in shape:
            colour = blend(colour, (255, 255, 230), 0.30)   # light from the upper left
        elif (x + 1, y) not in shape or (x, y + 1) not in shape:
            colour = blend(colour, (0, 0, 0), 0.35)
        filled[(x, y)] = colour
    x1, x2, ty = TEXT_LINE
    for x in range(x1, x2 + 1):                      # the pressed line: dark, lit on its lower edge
        filled[(x, ty)] = blend(filled[(x, ty)], edge, 0.55)
        filled[(x, ty + 1)] = blend(filled[(x, ty + 1)], (255, 255, 230), 0.25)
    outline = set()
    for x, y in filled:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if (x + dx, y + dy) not in filled:
                    outline.add((x + dx, y + dy))
    shadow = {(x + 1, y + 1) for x, y in outline} - set(filled) - outline   # soft shadow to the lower right
    for x, y in shadow:
        p = at(pixels, x, top + y)
        p[0], p[1], p[2] = blend((p[2], p[1], p[0]), edge, 0.45)[::-1]
    for x, y in outline:
        p = at(pixels, x, top + y)
        p[0], p[1], p[2] = blend((p[2], p[1], p[0]), edge, 0.80)[::-1]
    for (x, y), (r, g, b) in filled.items():
        p = at(pixels, x, top + y)
        p[0], p[1], p[2], p[3] = b, g, r, 255


def png(pixels, path, scale=4):
    rows = b""
    for y in range(H):
        row = b""
        for x in range(W):
            b, g, r, a = at(pixels, x, y)
            bg = 40 if (x // 4 + y // 4) % 2 else 60
            row += bytes(round((c * a + bg * (255 - a)) / 255) for c in (r, g, b)) * scale
        rows += (b"\0" + row) * scale

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    head = struct.pack(">IIBBBBB", W * scale, H * scale, 8, 2, 0, 0, 0)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", head) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def main():
    prefix, header, pixels, footer = read(SOURCE)
    others = [read(os.path.join(DIR, f"newui_chainfo_btn_{name}.OZT"))[2] for name in OTHERS]
    for top, colours, edge in ((0, GOLD, GOLD_EDGE), (HALF, GREY, GREY_EDGE)):
        paint_out_glyph(pixels, top, others)
        draw_chart(pixels, top, colours, edge)
    open(TARGET, "wb").write(prefix + header + b"".join(bytes(p) for p in pixels) + footer)
    print("wrote", os.path.relpath(TARGET, ROOT))
    if "--png" in sys.argv:
        png(pixels, sys.argv[sys.argv.index("--png") + 1])


if __name__ == "__main__":
    main()
