"""Draws the top of the character window with a name plate, Data/Interface/newui_chainfo_top.OZT (MU Skynet,
ghinzuro 2026-10-29: "некрасивый черный фон" behind the name in the C window).

The window's top is the original newui_item_back04 (190x64): a dark metal bar with silver scrolls on both sides and
a flat black box (x 56-132, y 13-25) between the scrolls' tips, where the name stands. This keeps everything else
of that texture and puts a plate in the frame's manner in the box's place: an outer black line, a silver line (light
from above: brighter on top, darker below), a dark inner line, chamfered corners, and a dark steel fill that is a
little lighter in the middle, behind the name, and darker towards the ends; a silver diamond stud on each end,
where the scrolls' tips point. The name stands between the studs' inner tips (x 59-129, character_info.rcss).

    python -I tools/chainfo_name_plate.py           write the file
    python -I tools/chainfo_name_plate.py --png X   also write a 4x preview to X

An OZT is 4 bytes and a TGA: uncompressed 32-bit BGRA, rows bottom up, then the TGA 2.0 footer.
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, "src", "bin", "Data", "Interface")
SOURCE = os.path.join(DIR, "newui_item_back04.OZT")
TARGET = os.path.join(DIR, "newui_chainfo_top.OZT")
W, H = 190, 64

# The plate's outer box (inclusive), in image coordinates (y down): the black box grown by one pixel on each side,
# so its outline sits where the bar's own dark rows around the box were.
LEFT, RIGHT, TOP, BOTTOM = 55, 133, 12, 26
CHAMFER = 2                                      # corners cut by 2 pixels

OUTLINE = (6, 6, 7)
SILVER_TOP, SILVER_BOTTOM = (196, 196, 200), (104, 104, 110)   # the scrolls' silver, lit from above
INNER_LINE = (12, 12, 14)
FILL_MIDDLE = ((44, 48, 56), (30, 33, 39))       # behind the name: top, bottom
FILL_ENDS = ((16, 17, 20), (10, 10, 12))         # at the plate's ends: top, bottom
STUD = 3                                         # the diamonds on the plate's ends: radius
STUD_LIGHT, STUD_DARK = (226, 226, 230), (86, 86, 92)


def read(path):
    raw = open(path, "rb").read()
    tga = raw[4:]
    w, h, bpp = struct.unpack_from("<HHB", tga, 12)
    assert (w, h, bpp, tga[2]) == (W, H, 32, 2), "not a 190x64 uncompressed 32-bit texture"
    start = 18 + tga[0]
    pixels = [list(tga[start + i * 4:start + i * 4 + 4]) for i in range(W * H)]   # BGRA
    return raw[:4], tga[:start], pixels, tga[start + W * H * 4:]


def put(pixels, x, y, rgb):
    """Sets the pixel at (x, y) counted from the top of the image."""
    pixels[(H - 1 - y) * W + x][:] = [rgb[2], rgb[1], rgb[0], 255]


def blend(c1, c2, t):
    return tuple(round(a + (b - a) * t) for a, b in zip(c1, c2))


def ring(x, y):
    """How deep (x, y) lies in the chamfered plate: 0 the outline, 1 the silver, 2 the inner line, 3+ the fill;
    None outside."""
    if not (LEFT <= x <= RIGHT and TOP <= y <= BOTTOM):
        return None
    dx = min(x - LEFT, RIGHT - x)
    dy = min(y - TOP, BOTTOM - y)
    # A cut corner: the diagonal's distance counts like the straight edges'.
    d = min(dx, dy, dx + dy - CHAMFER)
    return d if d >= 0 else None


def draw(pixels):
    inner_top, inner_bottom = TOP + 3, BOTTOM - 3
    middle = (LEFT + RIGHT) / 2
    half = (RIGHT - LEFT) / 2 - 3
    for y in range(TOP, BOTTOM + 1):
        down = (y - TOP) / (BOTTOM - TOP)
        for x in range(LEFT, RIGHT + 1):
            d = ring(x, y)
            if d is None:
                continue
            if d == 0:
                colour = OUTLINE
            elif d == 1:
                colour = blend(SILVER_TOP, SILVER_BOTTOM, down)
            elif d == 2:
                colour = INNER_LINE
            else:
                t = (y - inner_top) / max(1, inner_bottom - inner_top)
                # Lighter in the middle, behind the name: a soft bell across the plate.
                across = min(1.0, abs(x - middle) / half)
                glow = (1 - across * across) ** 1.5
                ends = blend(FILL_ENDS[0], FILL_ENDS[1], t)
                mid = blend(FILL_MIDDLE[0], FILL_MIDDLE[1], t)
                colour = blend(ends, mid, glow)
                # A faint sheen right under the top line, as on the bar itself.
                if y == inner_top:
                    colour = blend(colour, (90, 92, 100), 0.35 * glow + 0.1)
            put(pixels, x, y, colour)
    for cx in (LEFT, RIGHT):
        draw_stud(pixels, cx, (TOP + BOTTOM) // 2)


def draw_stud(pixels, cx, cy):
    """A silver diamond on the plate's end, where a scroll's tip points: a black outline, silver lit from the upper
    left, a bright point."""
    for y in range(cy - STUD - 1, cy + STUD + 2):
        for x in range(cx - STUD - 1, cx + STUD + 2):
            d = abs(x - cx) + abs(y - cy)
            if d > STUD + 1:
                continue
            if d == STUD + 1:
                colour = OUTLINE
            else:
                # Light from the upper left: brighter where x + y is smaller.
                t = ((x - cx) + (y - cy) + STUD) / (2 * STUD)
                colour = blend(STUD_LIGHT, STUD_DARK, max(0.0, min(1.0, t)))
            put(pixels, x, y, colour)
    put(pixels, cx - 1, cy - 1, (240, 240, 244))


def write_png(path, pixels, scale):
    rows = []
    for y in range(H):
        row = bytearray()
        for x in range(W):
            b, g, r, a = pixels[(H - 1 - y) * W + x]
            # Over the window's brown fill, as in the game.
            bg = (74, 52, 36)
            px = bytes(round(c * a / 255 + k * (255 - a) / 255) for c, k in zip((r, g, b), bg))
            row += px * scale
        rows += [bytes(row)] * scale
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", W * scale, H * scale, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    head, header, pixels, tail = read(SOURCE)
    draw(pixels)
    with open(TARGET, "wb") as f:
        f.write(head + header + b"".join(bytes(p) for p in pixels) + tail)
    print("wrote", TARGET)
    if "--png" in sys.argv:
        path = sys.argv[sys.argv.index("--png") + 1]
        write_png(path, pixels, 4)
        print("wrote", path)


if __name__ == "__main__":
    main()
