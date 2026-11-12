#!/usr/bin/env python3
"""Writes colour grading lookup tables for post-processing (docs/post-processing.md).

A table is a 32x32x32 colour cube stored as a 1024x32 PNG strip: 32 slices of
32x32 side by side, red along x inside a slice, green along y, blue picking the
slice. The client reads src/bin/Data/Grading/World<N>.png for map N-1.

`identity` writes the table that changes nothing: the starting point for a
table graded in an image editor (grade a screenshot with the strip pasted into
it, then cut the strip out again). The named presets are the tables shipped in
src/bin/Data/Grading, generated here so they can be reproduced and tuned.

Every table written here obeys two rules, checked before the file is written:
it never makes a colour brighter (Rec. 709 luma), and it leaves highlights
alone (no tint above HIGHLIGHT_LUMA), so armour, glows and light pools keep
their Classic colour. Grading may cool and deepen shadows a little; real base
darkness belongs to the map's light, not to its grading.

The Remaster client (HD textures on) reads its own tables from Data/Grading/Remaster: the
map's Classic mood followed by the Remaster tone (lighter mid-tones, an S curve, richer colour,
warm light and cool shadows; variant "3 rich" picked by ghinzuro 2026-11-12). Those tables may
brighten and do touch highlights, so the Classic rules above do not apply to them; Default.png
there grades every map without its own Remaster table.

Usage: scripts/grading_table.py identity|<map preset> <output.png>
       scripts/grading_table.py maps src/bin/Data/Grading   (every shipped map table)
       scripts/grading_table.py remaster src/bin/Data/Grading/Remaster   (the Remaster tables)
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path

SIZE = 32
LUMA = (0.2126, 0.7152, 0.0722)


def luma(color):
    return sum(weight * channel for weight, channel in zip(LUMA, color))


def clamp(value):
    return min(max(value, 0.0), 1.0)


def identity(color):
    return color


# Above this luma a table must leave colours as they are.
HIGHLIGHT_LUMA = 0.6
# How far a channel may move above HIGHLIGHT_LUMA, and how much brighter any colour may get
# (one 8-bit level: rounding).
TOLERANCE = 1.0 / 255.0


def shadow_weight(y):
    """1 at black, falling smoothly to 0 at HIGHLIGHT_LUMA."""
    t = clamp(1.0 - y / HIGHLIGHT_LUMA)
    return t * t * (3.0 - 2.0 * t)


def mood(shadow_tint, desaturation=0.0, deepening=0.0, saturation=0.0):
    """A grade that tints, (de)saturates and deepens the shadows and leaves highlights alone.

    shadow_tint is added per channel at black and fades out towards HIGHLIGHT_LUMA, like the
    other two. desaturation pulls a shadow towards its grey, saturation pushes it away from it
    (both 0..1); deepening darkens black-side colours by that fraction of their luma.
    """
    def grade(color):
        y = luma(color)
        w = shadow_weight(y)
        spread = (saturation - desaturation) * w
        toned = [channel + (channel - y) * spread + tint * w for channel, tint in zip(color, shadow_tint)]
        # Brightness: the shadows a little deeper, everything else where it was. Clamping a channel
        # undoes part of the correction, so correct again until it holds.
        target = y * (1.0 - deepening * w)
        for _ in range(4):
            correction = target - luma(toned)
            toned = [clamp(channel + correction) for channel in toned]
        return toned

    return grade


# Cold, slightly desaturated and a little deeper shadows; mid-tones and highlights untouched.
dungeon = mood((-0.025, 0.004, 0.030), desaturation=0.25, deepening=0.12)


def check(grade):
    """The table rules: never brighter, highlights untouched. Returns the worst offences."""
    brighter = 0.0
    highlight_shift = 0.0
    steps = [index / (SIZE - 1) for index in range(SIZE)]
    for red in steps:
        for green in steps:
            for blue in steps:
                color = [red, green, blue]
                graded = grade(color)
                brighter = max(brighter, luma(graded) - luma(color))
                if luma(color) >= HIGHLIGHT_LUMA:
                    highlight_shift = max(highlight_shift, max(abs(g - c) for g, c in zip(graded, color)))
    return brighter, highlight_shift


# The map tables in src/bin/Data/Grading: preset name, then the World<N>.png files it is
# shipped as (N = map number + 1). Every mood stays within the table rules below.
MAP_TABLES = {
    # Lorencia: warm evening shadows, a touch more colour.
    "lorencia": (mood((0.022, 0.008, -0.020), saturation=0.10, deepening=0.04), [1]),
    "dungeon": (dungeon, [2]),
    # Devias: snow light, blue shadows.
    "devias": (mood((-0.020, 0.002, 0.028), desaturation=0.10, deepening=0.05), [3]),
    # Noria: green forest shade, slightly richer.
    "noria": (mood((-0.010, 0.018, -0.010), saturation=0.12, deepening=0.04), [4]),
    # Lost Tower: violet gloom, deeper.
    "losttower": (mood((0.014, -0.012, 0.026), desaturation=0.15, deepening=0.12), [5]),
    # Atlans: underwater teal.
    "atlans": (mood((-0.025, 0.012, 0.022), desaturation=0.10, deepening=0.06), [8]),
    # Tarkan: hot dusty amber, washed a little.
    "tarkan": (mood((0.026, 0.010, -0.028), desaturation=0.12, deepening=0.03), [9]),
    # Icarus: thin cold air above the clouds.
    "icarus": (mood((-0.014, 0.006, 0.022), desaturation=0.05), [11]),
    # Aida: deep emerald woods.
    "aida": (mood((-0.016, 0.016, 0.004), saturation=0.08, deepening=0.08), [34]),
    # Kanturu: cold steel ruins.
    "kanturu": (mood((-0.012, 0.000, 0.016), desaturation=0.22, deepening=0.08), [38, 39, 40]),
    # Swamp of Quiet: murky olive.
    "swamp": (mood((0.008, 0.012, -0.018), desaturation=0.18, deepening=0.10), [57]),
    # Raklion: icy blue, crisp.
    "raklion": (mood((-0.022, 0.006, 0.030), saturation=0.04, deepening=0.04), [58, 59]),
}

# The Remaster tone: gamma (below 1 lifts mid-tones), S-curve contrast around REMASTER_PIVOT,
# saturation (0.3 = 30 % further from grey), warmth (warm highlights, cool shadows).
REMASTER_PIVOT = 0.45


def remaster_tone(gamma=0.86, contrast=0.20, saturation=0.30, warmth=0.03):
    def s_curve(x):
        if x < REMASTER_PIVOT:
            return REMASTER_PIVOT * (x / REMASTER_PIVOT) ** (1.0 + contrast)
        return 1.0 - (1.0 - REMASTER_PIVOT) * ((1.0 - x) / (1.0 - REMASTER_PIVOT)) ** (1.0 + contrast)

    def grade(color):
        curved = [s_curve(clamp(channel) ** gamma) for channel in color]
        y = luma(curved)
        rich = [y + (channel - y) * (1.0 + saturation) for channel in curved]
        light = clamp((y - 0.5) / 0.5)
        shade = 1.0 - y
        warm = (light * warmth - shade * warmth * 0.15, light * warmth * 0.4, -light * warmth * 0.6 + shade * warmth * 0.2)
        return [clamp(channel + shift) for channel, shift in zip(rich, warm)]

    return grade


def then(first, second):
    return lambda color: second(first(color))


# The Remaster tone per map: maps listed here use their own numbers, every other map the default
# (on top of its Classic mood where it has one). Tuned from in-game shots.
REMASTER_DEFAULT = remaster_tone()
REMASTER_TONES = {}


def remaster_tables():
    """(name, grade, file name) for every Remaster table: one per map with a Classic table, plus Default."""
    jobs = [("default", REMASTER_DEFAULT, "Default.png")]
    for name, (mood_grade, numbers) in MAP_TABLES.items():
        tone = REMASTER_TONES.get(name, REMASTER_DEFAULT)
        jobs += [(f"remaster {name}", then(mood_grade, tone), f"World{number}.png") for number in numbers]
    return jobs


PRESETS = {"identity": identity, **{name: grade for name, (grade, _) in MAP_TABLES.items()}}


def table_rows(grade):
    rows = []
    for green in range(SIZE):
        row = bytearray()
        for blue in range(SIZE):
            for red in range(SIZE):
                color = [red / (SIZE - 1), green / (SIZE - 1), blue / (SIZE - 1)]
                graded = grade(color)
                row += bytes(round(channel * 255) for channel in graded) + b"\xff"
        rows.append(bytes(row))
    return rows


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def write_png(path, rows):
    width = len(rows[0]) // 4
    header = struct.pack(">IIBBBBB", width, len(rows), 8, 6, 0, 0, 0)  # 8-bit RGBA
    raw = b"".join(b"\x00" + row for row in rows)
    data = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header) + png_chunk(b"IDAT", zlib.compress(raw, 9))
            + png_chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("preset", choices=sorted(PRESETS) + ["maps", "remaster"])
    parser.add_argument("output", type=Path, help="the PNG, or for maps / remaster the folder")
    arguments = parser.parse_args()
    if arguments.preset == "remaster":
        for _, grade, file_name in remaster_tables():
            write_png(arguments.output / file_name, table_rows(grade))
        return 0
    if arguments.preset == "maps":
        jobs = [(name, grade, arguments.output / f"World{number}.png")
                for name, (grade, numbers) in MAP_TABLES.items() for number in numbers]
    else:
        jobs = [(arguments.preset, PRESETS[arguments.preset], arguments.output)]
    for name, grade, output in jobs:
        brighter, highlight_shift = check(grade)
        if brighter > TOLERANCE or highlight_shift > TOLERANCE:
            print(f"{name} refused: brightens a colour by {brighter * 255:.1f} levels, moves a highlight by "
                  f"{highlight_shift * 255:.1f} levels (at most {TOLERANCE * 255:.0f} each)", file=sys.stderr)
            return 1
        write_png(output, table_rows(grade))
    return 0


if __name__ == "__main__":
    sys.exit(main())
