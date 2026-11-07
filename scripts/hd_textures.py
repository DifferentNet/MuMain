r"""HD textures for the MU client: OZJ/OZT files upscaled x4 by Real-ESRGAN.

The output files carry the client's upscale marker ('M' 'U' 'X' 4 in the first
4 bytes, Render/Textures/UpscaleMarker.h): the game uploads the big image but
keeps the original size for UVs and sprites. The client loads them from Data/HD
when the player switches HD textures on in Options (Render/Textures/HdTextures.h).

    python -I scripts/hd_textures.py --src <client>/Data --out <client>/Data/HD \
        --esrgan <realesrgan-ncnn-vulkan.exe> Player/Item3002_* World1/Tile*
    python -I scripts/hd_textures.py --src ... --out ... --esrgan ... --all

Patterns are relative to --src; --all takes every folder drawn in the world
(World*, Object*, Player, Item, Monster, NPC), not the interface. Files that
already carry the marker, or whose copy is in --out already, are skipped, so a
stopped run goes on where it stopped. Real-ESRGAN: realesrgan-ncnn-vulkan from
https://github.com/xinntao/Real-ESRGAN (BSD-3), model realesrgan-x4plus.
Tiles (World*/Tile*) are padded by wrapping before the upscale so their edges
still meet without a seam; other textures are padded by repeating the edge.
"""

import argparse
import io
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

SCALE = 4
MARKER = b"MUX" + bytes([SCALE])
OZJ_PREFIX = 24  # bytes in front of the JPEG
OZT_PREFIX = 4  # bytes in front of the TGA
JPEG_QUALITY = 92
# The folders --all takes: everything drawn in the world.
WORLD_FOLDERS = re.compile(r"^(world|object)\d+$|^(player|item|monster|npc)$", re.IGNORECASE)


def is_tile(rel: Path) -> bool:
    # Ground tiles repeat in both directions. The OZT "tiles" are grass blade strips with
    # transparency, which do not repeat top to bottom.
    return (rel.parts[0].lower().startswith("world") and rel.name.lower().startswith("tile")
            and rel.suffix.lower() == ".ozj")


def read_texture(path: Path) -> Image.Image:
    data = path.read_bytes()
    if path.suffix.lower() == ".ozj":
        return Image.open(io.BytesIO(data[OZJ_PREFIX:])).convert("RGB")
    return Image.open(io.BytesIO(data[OZT_PREFIX:])).convert("RGBA")


def pad(image: Image.Image, amount: int, wrap: bool) -> Image.Image:
    if not wrap:
        return _edge_pad(image, amount)
    w, h = image.size
    big = Image.new(image.mode, (w * 3, h * 3))
    for dx in range(3):
        for dy in range(3):
            big.paste(image, (dx * w, dy * h))
    return big.crop((w - amount, h - amount, 2 * w + amount, 2 * h + amount))


def _edge_pad(image: Image.Image, amount: int) -> Image.Image:
    w, h = image.size
    out = Image.new(image.mode, (w + 2 * amount, h + 2 * amount))
    out.paste(image, (amount, amount))
    # Repeat the edge rows and columns outwards.
    out.paste(image.crop((0, 0, w, 1)).resize((w, amount)), (amount, 0))
    out.paste(image.crop((0, h - 1, w, h)).resize((w, amount)), (amount, h + amount))
    left = out.crop((amount, 0, amount + 1, h + 2 * amount)).resize((amount, h + 2 * amount))
    right = out.crop((w + amount - 1, 0, w + amount, h + 2 * amount)).resize((amount, h + 2 * amount))
    out.paste(left, (0, 0))
    out.paste(right, (w + amount, 0))
    return out


def write_ozj(path: Path, image: Image.Image) -> None:
    buffer = io.BytesIO()
    image.save(buffer, "JPEG", quality=JPEG_QUALITY, subsampling=0, optimize=True)
    jpeg = buffer.getvalue()
    # The 24 bytes in front are never read; the original repeats the JPEG's start there.
    path.write_bytes(MARKER + jpeg[len(MARKER):OZJ_PREFIX] + jpeg)


def write_ozt(path: Path, image: Image.Image) -> None:
    w, h = image.size
    header = bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + w.to_bytes(2, "little") + h.to_bytes(2, "little") \
        + bytes([32, 8])
    r, g, b, a = image.split()
    pixels = Image.merge("RGBA", (b, g, r, a)).transpose(Image.FLIP_TOP_BOTTOM).tobytes()
    path.write_bytes(MARKER + header + pixels)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--esrgan", type=Path, required=True)
    ap.add_argument("--model", default="realesrgan-x4plus")
    ap.add_argument("--pad", type=int, default=16, help="source pixels of padding around each texture")
    ap.add_argument("--all", action="store_true", help="every world, object, player, item, monster and NPC folder")
    ap.add_argument("patterns", nargs="*")
    args = ap.parse_args()

    patterns = list(args.patterns)
    if args.all:
        patterns += [f"{d.name}/**/*" for d in args.src.iterdir() if d.is_dir() and WORLD_FOLDERS.match(d.name)]
    if not patterns:
        ap.error("give patterns or --all")
    matched = {p for pattern in patterns for p in args.src.glob(pattern)
               if p.is_file() and p.suffix.lower() in (".ozj", ".ozt")}
    files = sorted(p for p in matched if not (args.out / p.relative_to(args.src)).exists())
    if matched and not files:
        print("every copy is there already")
        return 0
    if not files:
        print("no OZJ/OZT files match", file=sys.stderr)
        return 1

    work = Path(tempfile.mkdtemp(prefix="hdtex-"))
    try:
        inputs, outputs = work / "in", work / "out"
        inputs.mkdir()
        outputs.mkdir()
        jobs = []
        for index, path in enumerate(files):
            rel = path.relative_to(args.src)
            if path.read_bytes()[:3] == MARKER[:3]:
                print(f"skip (already marked): {rel}")
                continue
            image = read_texture(path)
            padded = pad(image, args.pad, is_tile(rel))
            padded.convert("RGB").save(inputs / f"{index:05d}_rgb.png")
            if image.mode == "RGBA":
                alpha = padded.getchannel("A")
                Image.merge("RGB", (alpha, alpha, alpha)).save(inputs / f"{index:05d}_a.png")
            jobs.append((index, rel, image.mode, image.size))

        model_dir = args.esrgan.parent / "models"
        subprocess.run([str(args.esrgan), "-i", str(inputs), "-o", str(outputs), "-n", args.model,
                        "-s", str(SCALE), "-m", str(model_dir), "-f", "png"], check=True,
                       stdout=subprocess.DEVNULL)

        p = args.pad * SCALE
        for index, rel, mode, (w, h) in jobs:
            box = (p, p, p + w * SCALE, p + h * SCALE)
            rgb = Image.open(outputs / f"{index:05d}_rgb.png").convert("RGB").crop(box)
            target = args.out / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            if mode == "RGBA":
                alpha = Image.open(outputs / f"{index:05d}_a.png").convert("L").crop(box)
                rgba = rgb.copy()
                rgba.putalpha(alpha)
                write_ozt(target, rgba)
            else:
                write_ozj(target, rgb)
            before = (args.src / rel).stat().st_size
            print(f"{rel}: {w}x{h} -> {w * SCALE}x{h * SCALE}, {before // 1024} -> {target.stat().st_size // 1024} KB")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
