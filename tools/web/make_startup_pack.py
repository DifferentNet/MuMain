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
"""
import argparse
import gzip
import hashlib
import json
import os
import shutil
import sys

# The page asks for "<name>.gz" first and takes the file itself where there is none.
GZIP_COPIES = ["startup.pack", "Main.wasm"]
CHUNK = 1 << 20


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
    return 0


if __name__ == "__main__":
    sys.exit(main())
