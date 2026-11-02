"""Packs the game files the web client reads before it can be played, and writes the release's index.

    python -I tools/web/make_startup_pack.py --root <web output> --list src/web/startup-files.txt

Runs after cmake/WriteWebFileList.cmake: files.txt, every file the browser may read, says which files
there are, and the files this script writes are added to it, so scripts/publish-web.py uploads them.

The startup pack. Without it, the browser fetches each file the moment the game opens it, one after
another, and the game waits for every one: thousands of requests before the login screen. The page
downloads the pack while the game's code loads, and WebFileSystem.cpp puts its files in place before
the game starts. The list holds paths relative to the web output, one per line; '#' starts a comment,
and paths missing from the output are skipped with a note. The pack comes in parts, one per folder
(packs/startup-data-player.pack.gz ...; small folders share packs/startup-misc.pack.gz): the page
keeps each part in the browser's storage under its hash, so a release that changes a few files
downloads the parts holding them, not all 115 MB again.

A pack per map: packs/worldN.pack.gz with the files of Data/WorldN and Data/ObjectN (the map's ground
and objects) that aren't in the startup pack, and packs/worldN.json, their paths and sizes and a hash.
Entering Lorencia opened about 250 of them one after another, a minute on a phone (2026-10-29); the
page now downloads the map's pack when the game enters it and keeps it like the startup pack
(WebFileSystem.cpp LoadWorldFiles).

Packs are gzip files, which the page unpacks itself: the web server sends files as they are, and a
phone on a slow line waits for every megabyte. So is Main.wasm.gz, a copy of Main.wasm. A pack whose
hash didn't change keeps its file, so a rebuild doesn't compress it again and scripts/publish-web.py
doesn't upload it again.

Packs hold the models (.bmd) of version 12 decrypted, as version 10, which the game opens as they are
(ZzzBMD.cpp Open2). Encrypted bytes hardly compress: the startup pack's models take 13.5 MB this way,
28.5 MB as they are. The files' sizes in the index are the packed ones.

startup.json, which the page reads first:
  parts     the startup pack's parts: [{name, hash, packed (bytes to download), files: [[path, size]]}]
  code      the hashes of Main.js and Main.wasm: the page asks for Main.js?v=<hash> ...
  versions  {path: hash} of the files the game downloads one by one when it first reads them
            (fetch-backend.js asks for path?v=<hash>)
A URL with ?v=<hash> names one version of a file, so the page's service worker (sw.js) keeps it on
the device and asks the server again only when the hash changes. Files beside Main.js get none:
scripts/publish-web.py rewrites config.ini for the server it publishes to.
"""
import argparse
import gzip
import hashlib
import json
import os
import re
import shutil
import sys

# The page asks for "<name>.gz" first and takes the file itself where there is none.
GZIP_COPIES = ["Main.wasm"]
CODE = ["Main.js", "Main.wasm"]
# The single startup pack of the builds before the parts.
OLD_STARTUP_PACK = ["startup.pack", "startup.pack.gz"]
CHUNK = 1 << 20
HASH_LENGTH = 16
# A map's folders: Data/World1 and Data/Object1 are Lorencia's (MapManager.cpp LoadWorld); a few
# maps' folders are lower case (Data/world55).
MAP_FOLDER = re.compile(r"(World|Object)(\d+)", re.IGNORECASE)
PACKS = "packs"
# Folders with less of the startup pack than this share one part.
PART_MIN_BYTES = 2_000_000
# Changes every pack's hash when what the packs hold changes, not only the files.
PACK_FORMAT = b"plain-models"

# A model's header: "BMD", its version, and for version 12 the size of the encrypted rest (int32).
MODEL_SUFFIX = ".bmd"
MODEL_ENCRYPTED = b"BMD\x0c"
MODEL_PLAIN = b"BMD\x0a"
MODEL_HEADER_BYTES = 8
# ZzzLodTerrain.h MapFileDecrypt.
MAP_XOR_KEY = bytes([0xD1, 0x73, 0x52, 0xF6, 0xD2, 0x9A, 0xCB, 0x27, 0x3E, 0xAF, 0x59, 0x31, 0x37, 0xB3, 0xE7, 0xA2])
MAP_FIRST_KEY = 0x5E
MAP_KEY_STEP = 0x3D
BYTE = 0x100


def decrypt_map_file(data):
    """MapFileDecrypt: byte i is ((data[i] ^ MAP_XOR_KEY[i % 16]) - key) mod 256, where key is
    data[i - 1] + MAP_KEY_STEP (MAP_FIRST_KEY for the first byte). No byte depends on another's result,
    so this works on whole numbers holding all the bytes, a byte apiece, in a fraction of a second."""
    count = len(data)
    if not count:
        return b""
    keys = (MAP_XOR_KEY * (count // len(MAP_XOR_KEY) + 1))[:count]
    step = bytes((value + MAP_KEY_STEP) % BYTE for value in range(BYTE))
    first = bytes([(MAP_FIRST_KEY - MAP_KEY_STEP) % BYTE])
    xored = int.from_bytes(data, "little") ^ int.from_bytes(keys, "little")
    subtracted = int.from_bytes((first + data[:-1]).translate(step), "little")
    # Each byte's subtraction without a borrow from the next: the minuend's top bits set and the
    # subtrahend's cleared keep every byte's difference within it, and the true top bits go back in.
    top_bits = int.from_bytes(b"\x80" * count, "little")
    all_bits = (1 << (8 * count)) - 1
    difference = (xored | top_bits) - (subtracted & (all_bits ^ top_bits))
    return (difference ^ ((xored ^ subtracted ^ all_bits) & top_bits)).to_bytes(count, "little")


def encrypted_model_length(path):
    """The length of a version 12 model's encrypted part, or None for any other file."""
    if not path.lower().endswith(MODEL_SUFFIX):
        return None
    with open(path, "rb") as f:
        header = f.read(MODEL_HEADER_BYTES)
    if len(header) < MODEL_HEADER_BYTES or header[:len(MODEL_ENCRYPTED)] != MODEL_ENCRYPTED:
        return None
    declared = int.from_bytes(header[len(MODEL_ENCRYPTED):], "little", signed=True)
    return max(0, min(declared, os.path.getsize(path) - MODEL_HEADER_BYTES))


def packed_size(path):
    """The bytes the file takes in a pack: a version 12 model loses its size field."""
    length = encrypted_model_length(path)
    return os.path.getsize(path) if length is None else len(MODEL_PLAIN) + length


def copy_packed(path, out):
    """Writes the file into a pack, a version 12 model decrypted."""
    length = encrypted_model_length(path)
    with open(path, "rb") as source:
        if length is None:
            shutil.copyfileobj(source, out, CHUNK)
            return
        source.seek(MODEL_HEADER_BYTES)
        out.write(MODEL_PLAIN + decrypt_map_file(source.read(length)))


def file_hash(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(CHUNK), b""):
            digest.update(block)
    return digest.hexdigest()[:HASH_LENGTH]


def gzip_copy(path):
    """Writes path + '.gz' unless it is already newer than path. The gzip header holds no name and
    no time, so the same file always gives the same bytes. Returns whether it wrote one."""
    target = path + ".gz"
    if not os.path.isfile(path):
        return False
    if os.path.isfile(target) and os.path.getmtime(target) >= os.path.getmtime(path):
        return False
    with open(path, "rb") as source, open(target + ".tmp", "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=raw, mtime=0) as out:
            shutil.copyfileobj(source, out, CHUNK)
    os.replace(target + ".tmp", target)
    return True


def read_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def write_pack(root, name, files, previous_hash):
    """Writes packs/<name>.pack.gz with the contents of files ([path, size], relative to root) unless
    previous_hash, the pack's hash in the last build, still matches them. Returns (its entry in the
    index, whether it wrote it)."""
    digest = hashlib.sha256(PACK_FORMAT + json.dumps(files).encode())
    for path, _ in files:
        with open(os.path.join(root, path), "rb") as f:
            for block in iter(lambda: f.read(CHUNK), b""):
                digest.update(block)
    pack_hash = digest.hexdigest()[:HASH_LENGTH]
    pack_path = os.path.join(root, PACKS, name + ".pack.gz")
    written = pack_hash != previous_hash or not os.path.isfile(pack_path)
    if written:
        with open(pack_path + ".tmp", "wb") as raw:
            with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=raw, mtime=0) as out:
                for path, _ in files:
                    copy_packed(os.path.join(root, path), out)
        os.replace(pack_path + ".tmp", pack_path)
    entry = {"name": f"{PACKS}/{name}.pack", "hash": pack_hash, "packed": os.path.getsize(pack_path), "files": files}
    return entry, written


def part_name(path):
    """The startup part a file goes in: its folder two levels down (Data/Player), else its top folder
    (fonts), as a file name (startup-data-player)."""
    folders = path.split("/")[:-1]
    return "startup-" + (re.sub(r"[^a-z0-9]+", "-", "-".join(folders[:2]).lower()).strip("-") or "root")


def startup_parts(files):
    """{part name: [[path, size], ...]} in the list's order (part_name; small folders share
    startup-misc)."""
    folders = {}
    for path, size in files:
        folders.setdefault(part_name(path), []).append([path, size])
    parts, misc = {}, []
    for name, folder_files in folders.items():
        if sum(size for _, size in folder_files) < PART_MIN_BYTES:
            misc.extend(folder_files)
        else:
            parts[name] = folder_files
    if misc:
        parts["startup-misc"] = misc
    return parts


def write_startup_parts(root, files, previous_index):
    """Returns (the parts' index entries, how many were written)."""
    previous = {part["name"]: part["hash"] for part in (previous_index or {}).get("parts", [])}
    entries, written = [], 0
    for name, part_files in startup_parts(files).items():
        entry, wrote = write_pack(root, name, part_files, previous.get(f"{PACKS}/{name}.pack"))
        entries.append(entry)
        written += wrote
    return entries, written


def write_map_packs(root, listed, in_startup_pack):
    """Writes a pack per map (the module's docstring). Returns (the packs' names, the paths in them,
    how many were written, megabytes in them)."""
    maps = {}
    for path in listed:
        folders = path.split("/")
        match = MAP_FOLDER.fullmatch(folders[1]) if len(folders) > 2 and folders[0] == "Data" else None
        if match and path not in in_startup_pack:
            maps.setdefault(int(match.group(2)), []).append([path, packed_size(os.path.join(root, path))])
    names, packed, written, total = [], set(), 0, 0
    for world, files in sorted(maps.items()):
        name = f"world{world}"
        index_path = os.path.join(root, PACKS, name + ".json")
        previous = read_json(index_path) or {}
        entry, wrote = write_pack(root, name, files, previous.get("hash"))
        if wrote or previous != entry:
            with open(index_path + ".tmp", "w", encoding="utf-8") as out:
                json.dump(entry, out, separators=(",", ":"))
            os.replace(index_path + ".tmp", index_path)
        names.append(name)
        packed.update(path for path, _ in files)
        written += wrote
        total += sum(size for _, size in files)
    return names, packed, written, total / 1e6


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", required=True, help="the web build's output folder")
    parser.add_argument("--list", required=True, help="the files to pack, relative to --root")
    args = parser.parse_args()

    list_path = os.path.join(args.root, "files.txt")
    with open(list_path, encoding="utf-8") as f:
        listed = [line.strip() for line in f if line.strip()]
    with open(args.list, encoding="utf-8") as f:
        wanted = [line.split("#", 1)[0].strip() for line in f]
    files, seen, missing = [], set(), 0
    for path in wanted:
        if not path or path in seen:
            continue
        seen.add(path)
        full = os.path.join(args.root, path)
        if os.path.isfile(full):
            files.append([path, packed_size(full)])
        else:
            missing += 1

    os.makedirs(os.path.join(args.root, PACKS), exist_ok=True)
    index_path = os.path.join(args.root, "startup.json")
    parts, parts_written = write_startup_parts(args.root, files, read_json(index_path))
    in_startup_pack = {path for path, _ in files}
    maps, in_map_packs, maps_written, map_megabytes = write_map_packs(args.root, listed, in_startup_pack)

    kept = {os.path.basename(part["name"]) + ".gz" for part in parts}
    kept.update(name + suffix for name in maps for suffix in (".pack.gz", ".json"))
    for leftover in os.listdir(os.path.join(args.root, PACKS)):
        if leftover not in kept:
            os.remove(os.path.join(args.root, PACKS, leftover))
    for old in OLD_STARTUP_PACK:
        if os.path.isfile(os.path.join(args.root, old)):
            os.remove(os.path.join(args.root, old))
    compressed = [name for name in GZIP_COPIES if gzip_copy(os.path.join(args.root, name))]

    in_packs = in_startup_pack | in_map_packs
    versions = {path: file_hash(os.path.join(args.root, path)) for path in listed
                if "/" in path and path not in in_packs and not path.startswith(PACKS + "/")}
    code = {name: file_hash(os.path.join(args.root, name)) for name in CODE
            if os.path.isfile(os.path.join(args.root, name))}
    with open(index_path + ".tmp", "w", encoding="utf-8") as out:
        json.dump({"parts": parts, "code": code, "versions": versions}, out, separators=(",", ":"))
    os.replace(index_path + ".tmp", index_path)

    written_files = sorted(f"{PACKS}/{name}" for name in kept) + [name + ".gz" for name in GZIP_COPIES]
    listed = sorted(set(listed) | {name for name in written_files if os.path.isfile(os.path.join(args.root, name))})
    with open(list_path, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(listed) + "\n")

    total = sum(size for _, size in files)
    packed = sum(part["packed"] for part in parts)
    note = f", {missing} listed files not in the output" if missing else ""
    print(f"Startup pack: {len(files)} files, {total / 1e6:.1f} MB in {len(parts)} parts "
          f"({packed / 1e6:.1f} MB packed), {parts_written} written{note}")
    for name in compressed:
        size = os.path.getsize(os.path.join(args.root, name + ".gz"))
        print(f"{name}.gz: {size / 1e6:.1f} MB")
    print(f"Map packs: {len(maps)}, {map_megabytes:.0f} MB in them, {maps_written} written")
    print(f"Files downloaded one by one, with versions: {len(versions)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
