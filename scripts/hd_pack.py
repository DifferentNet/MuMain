"""Packs the HD art (Data/HD, scripts/hd_art.py) for shipping: the same pictures in fewer bytes, nothing lost.

Usage: python -I scripts/hd_pack.py [--src <Data/HD>] [--dst <Data/HD-packed>] [--jobs N] [--check] [--no-normal-maps]

An OZT file is 4 bytes, an 18-byte TGA header, then uncompressed 32-bit BGRA pixels (Render/Textures/OztFile.h).
Packed, its pixels are a lossless WebP picture with `exact` on (colour under transparent pixels kept too); the
header and any bytes after the pixels are kept as they are. The game (Render/Textures/PackedOzt.cpp) rebuilds the
original file byte for byte before reading it. Layout, little endian:

    "MUW1"  u32 original size  u16 head size  u32 tail size  head  tail  WebP

Each packed file is decoded again here and compared with its source; a file that doesn't come back exactly, isn't
32-bit, or doesn't get smaller is copied as it is, as are OZJ files (already JPEG). Incremental: a file is redone
only when its source is newer or changed size; files whose source is gone are removed. Writes go to `<name>.tmp`
first, which publish-client.py never ships. --check only verifies every packed file against its source.

Normal maps: every upscaled (x2/x4) texture of a model (Item, Player, Monster, NPC, ObjectN) also gets its relief
map, built here once with the algorithm of the game's Render/Textures/NormalMaps.cpp, halved, and written next to
the texture as "<texture file>.nrm" (Render/Textures/NormalMapFile.h):

    "MUN1"  u16 width  u16 height  lossless WebP (red, green = tilt, 128 = flat; blue 255)

The game only reads them: a texture without one gets no relief. --no-normal-maps skips them.
"""
import argparse, io, os, re, struct, sys
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from PIL import Image

MAGIC = b"MUW1"
HEAD = 4 + 18
DEFAULT_SRC = r"E:\Mu Online Server\hd-art\Data\HD"
STAT_NAME = ".hd-pack-sources"  # src size and mtime per packed file, for the incremental runs
NRM_MAGIC = b"MUN1"
NRM_SUFFIX = ".nrm"
# Bumped when the maps' algorithm changes, so the next run builds them all again.
NRM_VERSION = "1"
MODEL_FOLDER = re.compile(r"^(item|player|monster|npc|object[0-9]+)$", re.IGNORECASE)
# NormalMaps.cpp: kFineStrength, kBroadStrength, kMaxTilt.
FINE_STRENGTH, BROAD_STRENGTH, MAX_TILT = 24.0, 12.0, 0.9


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


def wants_normal_map(rel):
    """NormalMaps::WantsNormalMap for a path under Data/HD: an OZT or OZJ of a model folder."""
    parts = rel.split("/")
    return len(parts) >= 2 and MODEL_FOLDER.match(parts[0]) is not None and rel.lower().endswith((".ozt", ".ozj"))


def texture_rgba(data, ozj):
    """(RGBA float32 array, height x width x 4, rows top down; scale) as the game uploads the HD texture,
    or None when the game builds no map for it (not marked x2/x4, unreadable)."""
    if len(data) < 24 or data[:3] != b"MUX" or data[3] not in (2, 4):
        return None
    scale = data[3]
    if ozj:
        with Image.open(io.BytesIO(data[24:])) as im:
            rgb = np.asarray(im.convert("RGB"), dtype=np.float32)
        alpha = np.full(rgb.shape[:2] + (1,), 255.0, dtype=np.float32)
        rgba = np.concatenate([rgb, alpha], axis=2)
    else:
        layout = pixels_layout(data)
        if layout is None:
            return None
        width, height = layout
        bgra = np.frombuffer(data, dtype=np.uint8, count=width * height * 4, offset=HEAD).reshape(height, width, 4)
        # The file's rows go from the bottom up and hold BGRA (OztFile.cpp CopyPixels).
        rgba = bgra[::-1, :, [2, 1, 0, 3]].astype(np.float32)
    height, width = rgba.shape[:2]
    if width < scale or height < scale:
        return None
    return rgba, scale


def box_blur(values, radius):
    """NormalMaps.cpp BoxBlur: the mean over (2r+1)^2 texels, edges clamped."""
    if radius <= 0:
        return values
    size = 2 * radius + 1
    padded = np.pad(values, radius, mode="edge").astype(np.float64)
    rows = np.cumsum(np.pad(padded, ((0, 0), (1, 0))), axis=1)
    rows = rows[:, size:] - rows[:, :-size]
    cols = np.cumsum(np.pad(rows, ((1, 0), (0, 0))), axis=0)
    return ((cols[size:, :] - cols[:-size, :]) / (size * size)).astype(np.float32)


def gradient(h, step):
    """NormalMaps.cpp Gradient: central differences over `step` texels, clamped at the edges, per texel."""
    height, width = h.shape
    xs = np.arange(width)
    x0, x1 = np.maximum(xs - step, 0), np.minimum(xs + step, width - 1)
    dx = np.maximum(x1 - x0, 1).astype(np.float32)
    gx = np.where(x1 > x0, (h[:, x1] - h[:, x0]) / dx, 0).astype(np.float32)
    ys = np.arange(height)
    y0, y1 = np.maximum(ys - step, 0), np.minimum(ys + step, height - 1)
    dy = np.maximum(y1 - y0, 1).astype(np.float32)[:, None]
    gy = np.where((y1 > y0)[:, None], (h[y1, :] - h[y0, :]) / dy, 0).astype(np.float32)
    return gx, gy


def tilt_of(rgba, scale):
    """The tilt (tx, ty) per texel of NormalMaps::Build, before its encoding to bytes."""
    lum = (0.299 * rgba[..., 0] + 0.587 * rgba[..., 1] + 0.114 * rgba[..., 2]) / 255.0
    h0 = (lum * (rgba[..., 3] / 255.0)).astype(np.float32)
    broad_radius = max(scale, 1) * 2
    fine = box_blur(h0, 1)
    broad = box_blur(fine, broad_radius)
    fx, fy = gradient(fine, 1)
    bx, by = gradient(broad, broad_radius)
    broad_scale = BROAD_STRENGTH * broad_radius
    # The surface rises where the image brightens, so the normal leans the other way.
    tx = -(fx * FINE_STRENGTH + bx * broad_scale)
    ty = -(fy * FINE_STRENGTH + by * broad_scale)
    inverse = 1.0 / np.sqrt(1.0 + tx * tx + ty * ty)
    tx, ty = tx * inverse, ty * inverse
    tilt = np.sqrt(tx * tx + ty * ty)
    limit = np.where(tilt > MAX_TILT, MAX_TILT / np.maximum(tilt, 1e-12), 1.0)
    return tx * limit, ty * limit


def encode_tilt(t):
    """NormalMaps.cpp Encode: 128 = flat, 1...255."""
    return np.clip(np.floor(128.0 + t * 127.0 + 0.5), 1, 255).astype(np.uint8)


def halve(values):
    """The mean of each 2 x 2 block; an odd last row or column counts with itself."""
    height, width = values.shape
    padded = np.pad(values, ((0, height % 2), (0, width % 2)), mode="edge")
    return 0.25 * (padded[0::2, 0::2] + padded[1::2, 0::2] + padded[0::2, 1::2] + padded[1::2, 1::2])


def normal_map(data, ozj):
    """The .nrm bytes of a texture file (its HD source, not packed), or None when it gets no map."""
    texture = texture_rgba(data, ozj)
    if texture is None:
        return None
    rgba, scale = texture
    tx, ty = tilt_of(rgba, scale)
    red, green = encode_tilt(halve(tx)), encode_tilt(halve(ty))
    height, width = red.shape
    rgb = np.dstack([red, green, np.full_like(red, 255)])
    buffer = io.BytesIO()
    Image.fromarray(rgb, "RGB").save(buffer, "WEBP", lossless=True, quality=100, method=5)
    return NRM_MAGIC + struct.pack("<HH", width, height) + buffer.getvalue()


def read_normal_map(data):
    """(width, height, RGB array) of a .nrm file."""
    if data[:4] != NRM_MAGIC:
        raise ValueError("not a normal map")
    width, height = struct.unpack_from("<HH", data, 4)
    with Image.open(io.BytesIO(data[8:])) as im:
        rgb = np.asarray(im.convert("RGB"))
    if rgb.shape[:2] != (height, width):
        raise ValueError("size differs")
    return width, height, rgb


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def one(job):
    """Packs or copies one file, or builds a normal map; returns (relative path, source size, written size, done?)."""
    rel, src, dst = job
    with open(src, "rb") as f:
        data = f.read()
    if rel.endswith(NRM_SUFFIX):
        nrm = normal_map(data, rel[:-len(NRM_SUFFIX)].lower().endswith(".ozj"))
        if nrm is None:
            if os.path.exists(dst):
                os.remove(dst)
            return rel, 0, 0, False
        write(dst, nrm)
        return rel, 0, len(nrm), True
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


def check(src, dst, normal_maps=True):
    bad = maps = 0
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
        nrm = out + NRM_SUFFIX
        if normal_maps and wants_normal_map(rel) and os.path.exists(nrm):
            try:
                with open(nrm, "rb") as f:
                    width, height, _ = read_normal_map(f.read())
                texture = texture_rgba(original, rel.lower().endswith(".ozj"))
                if texture is None or (height, width) != tuple((n + 1) // 2 for n in texture[0].shape[:2]):
                    raise ValueError("not half the texture's size")
                maps += 1
            except (OSError, ValueError) as e:
                bad += 1
                print("BROKEN MAP", rel + NRM_SUFFIX, e)
    print("check:", "all files come back exactly" if not bad else f"{bad} files differ", f"({maps} normal maps)")
    return bad == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--src", default=os.environ.get("MU_HD_SRC", DEFAULT_SRC))
    ap.add_argument("--dst")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--no-normal-maps", action="store_true", help="don't build the .nrm relief maps")
    args = ap.parse_args()
    src = os.path.abspath(args.src)
    dst = os.path.abspath(args.dst or src.rstrip("\\/") + "-packed")
    if not os.path.isdir(src):
        sys.exit(f"No HD art in {src}")
    if args.check:
        sys.exit(0 if check(src, dst, not args.no_normal_maps) else 1)

    stats = load_stats(dst)
    sources = dict(walk(src))
    # Each output file: its source and its name in the stats (a map's carries the algorithm's version).
    outputs = {rel: (path, rel) for rel, path in sources.items()}
    if not args.no_normal_maps:
        for rel, path in sources.items():
            if wants_normal_map(rel):
                outputs[rel + NRM_SUFFIX] = (path, f"{rel}{NRM_SUFFIX}@{NRM_VERSION}")
    jobs = []
    for rel, (path, stat_name) in sorted(outputs.items()):
        st = os.stat(path)
        key = (st.st_size, st.st_mtime_ns)
        out = os.path.join(dst, rel)
        # A texture that gets no map (not upscaled) leaves no file: its stats line alone says it is done.
        if stats.get(stat_name) == key and (os.path.isfile(out) or rel.endswith(NRM_SUFFIX)):
            continue
        jobs.append((rel, path, out))
    wanted = {stat_name for _, stat_name in outputs.values()}
    for name in [name for name in stats if name not in wanted]:
        stats.pop(name)
    removed = 0
    for rel, path in list(walk(dst)) if os.path.isdir(dst) else []:
        if rel not in outputs:
            os.remove(path)
            removed += 1

    print(f"{len(sources)} files, {len(outputs) - len(sources)} model textures for normal maps, "
          f"{len(jobs)} to do, {removed} removed")
    total_in = total_out = packed_count = map_count = map_bytes = 0
    with ProcessPoolExecutor(args.jobs) as pool:
        for i, (rel, size_in, size_out, done) in enumerate(pool.map(one, jobs, chunksize=4), 1):
            path, stat_name = outputs[rel]
            st = os.stat(path)
            stats[stat_name] = (st.st_size, st.st_mtime_ns)
            if rel.endswith(NRM_SUFFIX):
                map_count += done
                map_bytes += size_out
            else:
                total_in += size_in
                total_out += size_out
                packed_count += done
            if i % 200 == 0:
                save_stats(dst, stats)
                print(f"  {i}/{len(jobs)}  {total_in / 2**20:.0f} MB -> {total_out / 2**20:.0f} MB, "
                      f"{map_count} maps {map_bytes / 2**20:.0f} MB")
    save_stats(dst, stats)
    if jobs:
        print(f"done: {packed_count} OZT packed, {total_in / 2**20:.0f} MB -> {total_out / 2**20:.0f} MB; "
              f"{map_count} normal maps, {map_bytes / 2**20:.0f} MB")
    files = list(walk(dst))
    size = sum(os.path.getsize(p) for _, p in files)
    maps = sum(os.path.getsize(p) for rel, p in files if rel.endswith(NRM_SUFFIX))
    print(f"{dst}: {size / 2**20:.0f} MB in all, of which normal maps {maps / 2**20:.0f} MB "
          f"(source {sum(os.path.getsize(p) for p in sources.values()) / 2**20:.0f} MB)")


if __name__ == "__main__":
    main()
