"""Packs the HD art (Data/HD, scripts/hd_art.py) for shipping: the same pictures in fewer bytes, nothing lost.

Usage: python -I scripts/hd_pack.py [--src <Data/HD>] [--dst <Data/HD-packed>] [--jobs N] [--check]

An OZT file is 4 bytes, an 18-byte TGA header, then uncompressed 32-bit BGRA pixels (Render/Textures/OztFile.h).
Packed, its pixels are a lossless WebP picture with `exact` on (colour under transparent pixels kept too); the
header and any bytes after the pixels are kept as they are. The game (Render/Textures/PackedOzt.cpp) rebuilds the
original file byte for byte before reading it. Layout, little endian:

    "MUW1"  u32 original size  u16 head size  u32 tail size  head  tail  WebP

Each packed file is decoded again here and compared with its source; a file that doesn't come back exactly, isn't
32-bit, or doesn't get smaller is copied as it is, as are OZJ files (already JPEG). Incremental: a file is redone
only when its source is newer or changed size; files whose source is gone are removed. Writes go to `<name>.tmp`
first, which publish-client.py never ships. --check only verifies every packed file against its source.
"""
import argparse, io, os, struct, sys
from concurrent.futures import ProcessPoolExecutor

from PIL import Image

MAGIC = b"MUW1"
HEAD = 4 + 18
DEFAULT_SRC = r"E:\Mu Online Server\hd-art\Data\HD"
STAT_NAME = ".hd-pack-sources"  # src size and mtime per packed file, for the incremental runs


def pixels_layout(data):
    """(width, height) when data is an OZT the game reads (32 bit, every pixel there), else None."""
    if len(data) < HEAD:
        return None
    width, height = struct.unpack_from("<HH", data, 4 + 12)
    if data[4 + 16] != 32 or not width or not height or width > 16383 or height > 16383:
        return None
    if len(data) - HEAD < width * height * 4:
        return None
    return width, height


def unpack(packed):
    """The original file from a packed one (what the game does)."""
    if packed[:4] != MAGIC:
        raise ValueError("not packed")
    size, head_size, tail_size = struct.unpack_from("<IHI", packed, 4)
    at = 14
    head = packed[at:at + head_size]
    tail = packed[at + head_size:at + head_size + tail_size]
    webp = packed[at + head_size + tail_size:]
    with Image.open(io.BytesIO(webp)) as im:
        pixels = im.convert("RGBA").tobytes("raw", "BGRA")
    out = head + pixels + tail
    if len(out) != size:
        raise ValueError("size differs")
    return out


def pack(data):
    """The packed bytes of an OZT, or None when it should stay as it is."""
    layout = pixels_layout(data)
    if layout is None:
        return None
    width, height = layout
    end = HEAD + width * height * 4
    image = Image.frombytes("RGBA", (width, height), data[HEAD:end], "raw", "BGRA")
    buffer = io.BytesIO()
    image.save(buffer, "WEBP", lossless=True, exact=True, quality=100, method=5)
    head, tail = data[:HEAD], data[end:]
    packed = MAGIC + struct.pack("<IHI", len(data), len(head), len(tail)) + head + tail + buffer.getvalue()
    if len(packed) >= len(data) or unpack(packed) != data:
        return None
    return packed


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def one(job):
    """Packs or copies one file; returns (relative path, source size, written size, packed?)."""
    rel, src, dst = job
    with open(src, "rb") as f:
        data = f.read()
    packed = pack(data) if rel.lower().endswith(".ozt") else None
    write(dst, packed if packed is not None else data)
    return rel, len(data), len(packed if packed is not None else data), packed is not None


def walk(root):
    for folder, _, names in os.walk(root):
        for name in names:
            if name.endswith((".tmp", ".part")) or name == STAT_NAME:
                continue
            path = os.path.join(folder, name)
            yield os.path.relpath(path, root).replace("\\", "/"), path


def load_stats(dst):
    stats = {}
    try:
        with open(os.path.join(dst, STAT_NAME), encoding="utf-8") as f:
            for line in f:
                rel, size, mtime = line.rstrip("\n").rsplit("\t", 2)
                stats[rel] = (int(size), int(mtime))
    except FileNotFoundError:
        pass
    return stats


def save_stats(dst, stats):
    lines = "".join(f"{rel}\t{size}\t{mtime}\n" for rel, (size, mtime) in sorted(stats.items()))
    write(os.path.join(dst, STAT_NAME), lines.encode("utf-8"))


def check(src, dst):
    bad = 0
    for rel, path in walk(src):
        out = os.path.join(dst, rel)
        with open(path, "rb") as f:
            original = f.read()
        try:
            with open(out, "rb") as f:
                stored = f.read()
            same = (unpack(stored) if stored[:4] == MAGIC else stored) == original
        except (OSError, ValueError) as e:
            same = False
            print("ERROR", rel, e)
        if not same:
            bad += 1
            print("DIFFERS", rel)
    print("check:", "all files come back exactly" if not bad else f"{bad} files differ")
    return bad == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--src", default=os.environ.get("MU_HD_SRC", DEFAULT_SRC))
    ap.add_argument("--dst")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    src = os.path.abspath(args.src)
    dst = os.path.abspath(args.dst or src.rstrip("\\/") + "-packed")
    if not os.path.isdir(src):
        sys.exit(f"No HD art in {src}")
    if args.check:
        sys.exit(0 if check(src, dst) else 1)

    stats = load_stats(dst)
    sources = dict(walk(src))
    jobs = []
    for rel, path in sorted(sources.items()):
        st = os.stat(path)
        key = (st.st_size, st.st_mtime_ns)
        if stats.get(rel) == key and os.path.isfile(os.path.join(dst, rel)):
            continue
        jobs.append((rel, path, os.path.join(dst, rel)))
    removed = 0
    for rel, path in list(walk(dst)) if os.path.isdir(dst) else []:
        if rel not in sources:
            os.remove(path)
            stats.pop(rel, None)
            removed += 1

    print(f"{len(sources)} files, {len(jobs)} to do, {removed} removed")
    total_in = total_out = packed_count = 0
    with ProcessPoolExecutor(args.jobs) as pool:
        for i, (rel, size_in, size_out, packed) in enumerate(pool.map(one, jobs, chunksize=4), 1):
            st = os.stat(sources[rel])
            stats[rel] = (st.st_size, st.st_mtime_ns)
            total_in += size_in
            total_out += size_out
            packed_count += packed
            if i % 200 == 0:
                save_stats(dst, stats)
                print(f"  {i}/{len(jobs)}  {total_in / 2**20:.0f} MB -> {total_out / 2**20:.0f} MB")
    save_stats(dst, stats)
    if jobs:
        print(f"done: {packed_count} OZT packed, {total_in / 2**20:.0f} MB -> {total_out / 2**20:.0f} MB")
    size = sum(os.path.getsize(p) for _, p in walk(dst))
    print(f"{dst}: {size / 2**20:.0f} MB in all (source {sum(os.path.getsize(p) for p in sources.values()) / 2**20:.0f} MB)")


if __name__ == "__main__":
    main()
