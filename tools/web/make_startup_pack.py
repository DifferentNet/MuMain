"""Packs the game files the web client reads before it can be played into one download.

    python -I tools/web/make_startup_pack.py --root <web output> --list src/web/startup-files.txt

Without it, the browser fetches each file the moment the game opens it, one after another, and the
game waits for every one: thousands of requests before the login screen. The page downloads
startup.pack while the game's code loads, and WebFileSystem.cpp puts its files in place before the
game starts; files not in the pack still download when the game first opens them.

Writes <root>/startup.pack (the files, one after another) and <root>/startup.json (their paths and
sizes, in pack order). The list holds paths relative to the web output, one per line; '#' starts a
comment, and paths missing from the output are skipped with a note.
"""
import argparse
import json
import os
import sys


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
    with open(pack_path + ".tmp", "wb") as out:
        for path, _ in files:
            with open(os.path.join(args.root, path), "rb") as f:
                out.write(f.read())
    with open(os.path.join(args.root, "startup.json.tmp"), "w", encoding="utf-8") as out:
        json.dump({"files": files}, out, separators=(",", ":"))
    os.replace(pack_path + ".tmp", pack_path)
    os.replace(os.path.join(args.root, "startup.json.tmp"), os.path.join(args.root, "startup.json"))

    total = sum(size for _, size in files)
    note = f", {missing} listed files not in the output" if missing else ""
    print(f"startup.pack: {len(files)} files, {total / 1e6:.1f} MB{note}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
