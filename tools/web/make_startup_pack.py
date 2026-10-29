"""Packs the game files the web client reads before it can be played into one download.

    python -I tools/web/make_startup_pack.py --root <web output> --list src/web/startup-files.txt

Without it, the browser fetches each file the moment the game opens it, one after another, and the
game waits for every one: thousands of requests before the login screen. The page downloads
startup.pack while the game's code loads, and WebFileSystem.cpp puts its files in place before the
game starts; files not in the pack still download when the game first opens them.

Writes <root>/startup.pack (the files, one after another) and <root>/startup.json (their paths and
sizes, in pack order, and the pack's hash). The list holds paths relative to the web output, one per
line; '#' starts a comment, and paths missing from the output are skipped with a note.

Also writes gzip copies of startup.pack and Main.wasm (startup.pack.gz, Main.wasm.gz), which the
page downloads instead and unpacks itself: the web server sends files as they are, and a phone on a
slow line waits for every megabyte. The page keeps the pack in the browser's storage under its hash
and downloads it again only when the hash changes. An unchanged pack keeps its file, so a rebuild
doesn't compress it again and scripts/publish-web.py doesn't upload it again.

And writes a pack per map: packs/worldN.pack.gz with the files of Data/WorldN and Data/ObjectN (the
map's ground and objects) that aren't in startup.pack, and packs/worldN.json, their paths and sizes
and a hash. Entering Lorencia opened about 250 of them one after another, a minute on a phone
(2026-10-29); the page now downloads the map's pack when the game enters it and keeps it like
startup.pack (WebFileSystem.cpp LoadWorldFiles).
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
GZIP_COPIES = ["startup.pack", "Main.wasm"]
CHUNK = 1 << 20
# A map's folders: Data/World1 and Data/Object1 are Lorencia's (MapManager.cpp LoadWorld).
MAP_FOLDER = re.compile(r"(World|Object)(\d+)")
PACKS = "packs"


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


def write_map_pack(root, name, files):
    """Writes packs/<name>.pack.gz and packs/<name>.json for files ([path, size], relative to root)
    unless they hold them already. Returns whether it wrote them."""
    digest = hashlib.sha256(json.dumps(files).encode())
    for path, _ in files:
        with open(os.path.join(root, path), "rb") as f:
            for block in iter(lambda: f.read(CHUNK), b""):
                digest.update(block)
    index = {"files": files, "hash": digest.hexdigest()[:16]}
    pack_path = os.path.join(root, PACKS, name + ".pack.gz")
    index_path = os.path.join(root, PACKS, name + ".json")
    if read_json(index_path) == index and os.path.isfile(pack_path):
        return False
    with open(pack_path + ".tmp", "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=raw, mtime=0) as out:
            for path, _ in files:
                with open(os.path.join(root, path), "rb") as source:
                    shutil.copyfileobj(source, out, CHUNK)
    with open(index_path + ".tmp", "w", encoding="utf-8") as out:
        json.dump(index, out, separators=(",", ":"))
    os.replace(pack_path + ".tmp", pack_path)
    os.replace(index_path + ".tmp", index_path)
    return True


def write_map_packs(root, in_startup_pack):
    """Writes a pack per map (the module's docstring) and removes the packs of maps no longer there.
    Returns (packs, written, megabytes in them)."""
    folders = {}
    data = os.path.join(root, "Data")
    for entry in sorted(os.listdir(data)) if os.path.isdir(data) else []:
        match = MAP_FOLDER.fullmatch(entry)
        if match and os.path.isdir(os.path.join(data, entry)):
            folders.setdefault(int(match.group(2)), []).append(entry)
    os.makedirs(os.path.join(root, PACKS), exist_ok=True)
    kept, written, total = set(), 0, 0
    for world, names in sorted(folders.items()):
        files = []
        for folder in names:
            for path_dir, subfolders, file_names in os.walk(os.path.join(data, folder)):
                subfolders.sort()
                for file_name in sorted(file_names):
                    full = os.path.join(path_dir, file_name)
                    path = os.path.relpath(full, root).replace(os.sep, "/")
                    if path not in in_startup_pack:
                        files.append([path, os.path.getsize(full)])
        if not files:
            continue
        name = f"world{world}"
        written += write_map_pack(root, name, files)
        kept.update((name + ".pack.gz", name + ".json"))
        total += sum(size for _, size in files)
    for leftover in os.listdir(os.path.join(root, PACKS)):
        if leftover not in kept:
            os.remove(os.path.join(root, PACKS, leftover))
    return len(kept) // 2, written, total / 1e6


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", required=True, help="the web build's output folder")
    parser.add_argument("--list", required=True, help="the files to pack, relative to --root")
    args = parser.parse_args()

    with open(args.list, encoding="utf-8") as f:
        wanted = [line.split("#", 1)[0].strip() for line in f]
    files, seen, missing = [], set(), 0
    for path in wanted:
        if not path or path in seen:
            continue
        seen.add(path)
        full = os.path.join(args.root, path)
        if os.path.isfile(full):
            files.append((path, os.path.getsize(full)))
        else:
            missing += 1

    pack_path = os.path.join(args.root, "startup.pack")
    index_path = os.path.join(args.root, "startup.json")
    digest = hashlib.sha256()
    with open(pack_path + ".tmp", "wb") as out:
        for path, _ in files:
            with open(os.path.join(args.root, path), "rb") as f:
                data = f.read()
            digest.update(data)
            out.write(data)
    index = {"files": files, "hash": digest.hexdigest()[:16]}
    # Lists turn into lists, not tuples, so the index read back compares equal to an unchanged one.
    index = json.loads(json.dumps(index))

    total = sum(size for _, size in files)
    unchanged = read_json(index_path) == index and os.path.isfile(pack_path) and os.path.getsize(pack_path) == total
    if unchanged:
        os.remove(pack_path + ".tmp")
    else:
        with open(index_path + ".tmp", "w", encoding="utf-8") as out:
            json.dump(index, out, separators=(",", ":"))
        os.replace(pack_path + ".tmp", pack_path)
        os.replace(index_path + ".tmp", index_path)

    compressed = [name for name in GZIP_COPIES if gzip_copy(os.path.join(args.root, name))]
    note = f", {missing} listed files not in the output" if missing else ""
    state = "unchanged" if unchanged else "written"
    print(f"startup.pack: {len(files)} files, {total / 1e6:.1f} MB, {state}{note}")
    for name in compressed:
        size = os.path.getsize(os.path.join(args.root, name + ".gz"))
        print(f"{name}.gz: {size / 1e6:.1f} MB")

    packs, written, megabytes = write_map_packs(args.root, {path for path, _ in files})
    print(f"Map packs: {packs}, {megabytes:.0f} MB in them, {written} written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
