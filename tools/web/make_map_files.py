"""Writes src/web/map-files.txt, the files a map needs besides its own folders, for its pack, and
src/web/model-files.txt, the files the game reads right after a monster's or an NPC's model.

    python -I tools/web/make_map_files.py --root <web output> --spawns spawns.csv --trace trace.json

A map's pack (make_startup_pack.py) holds Data/WorldN and Data/ObjectN. The monsters and NPCs that
stand on the map, its minimap, its name's picture and its colour table are outside those folders: the
game downloaded each the moment it wanted it, one after another, and stood still meanwhile (a monster
seen for the first time reads some twenty files, its model, textures and sounds; on a phone that was
a freeze of a second or two, 2026-11-10). With map-files.txt the map's pack brings them too, behind the loading screen.

The list is made, not written by hand, from:
  --spawns  "map number,monster number" lines: who stands where on the server. From its database:
            select distinct m."Number", d."Number" from config."MonsterSpawnArea" s
            join config."GameMapDefinition" m on m."Id" = s."GameMapId"
            join config."MonsterDefinition" d on d."Id" = s."MonsterDefinitionId"
  --trace   {monster number: [model, file, ...]}: the files the game opened to show each one, from a
            build with MU_WEB_FILE_TRACE (WebFileSystem.cpp MuWeb_TraceMonster; tools/web/README.md).
            The game reads a file once in a run, so the files of a model's first monster count for
            every monster with that model.
  the source: the map names' pictures (UIMapName.cpp), in every language of the output.
  the output: Data/Local/<language>/Minimap/Minimap_World<N>_*, Data/Grading/World<N>.png.

A line of map-files.txt: "<pack's number> <path>". Monsters the server puts on a map by itself (an
event's, a summoned one) aren't in --spawns; the game downloads those when they show. For them
model-files.txt, "<model> <file>" lines from the same trace: the textures, sounds and further models
the game opened after a model. make_startup_pack.py puts them in startup.json ("groups"), and the page
downloads a model's files together instead of one after another (fetch-backend.js).

What the map of the trace run had already shown (its NPCs) opens nothing in the trace: take those from
a second run on another map, or from MuWeb_TraceTake right after entering.

The game opens some monsters' models only on their own maps (the Kalima ones); the trace of one map
has their model's number but no file. Those are named by the number, Data/Monster/Monster<NN>.bmd, and
get the textures the model names (model_textures.py).
"""
import argparse
import collections
import json
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_startup_pack  # noqa: E402
import model_textures  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAP_NAMES_SOURCE = "src/source/UI/HUD/UIMapName.cpp"
MAP_NAME = re.compile(r'm_mapImgPath\[(\d+)\]\s*=\s*strFolderName\s*\+\s*L"([^"]+)"')
MINIMAP = re.compile(r"Data/Local/[^/]+/Minimap/Minimap_World(\d+)[_.][^/]*", re.IGNORECASE)
GRADING = re.compile(r"Data/Grading/World(\d+)\.png", re.IGNORECASE)
# The files the game writes as .tga and .jpg are stored as .ozt and .ozj.
STORED_SUFFIX = {".tga": ".ozt", ".jpg": ".ozj", ".bmp": ".ozb"}
# Files in more packs than this are every map's: they belong in the startup or the late pack.
MOST_PACKS_OF_A_FILE = 12
MONSTER_MODEL = re.compile(r"Data/Monster/Monster(\d+)\.bmd", re.IGNORECASE)
MONSTER_MODEL_FILE = "Data/Monster/Monster{:02d}.bmd"
MODEL_SUFFIX = ".bmd"


def pack_of_map(number):
    """The number of the pack the game asks for on map `number` (MapManager.cpp LoadWorld: the folder's
    number, one more than the map's; the event maps share their first one's)."""
    if 11 <= number <= 17 or number == 52:
        return 12
    if 24 <= number <= 29 or number == 36:
        return 25
    if 18 <= number <= 23 or number == 53:
        return 19
    if 45 <= number <= 50:
        return 47
    return number + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", required=True, help="the web build's output folder")
    parser.add_argument("--spawns", required=True, help="'map,monster' lines from the server's database")
    parser.add_argument("--trace", required=True, help="{monster: [model, file, ...]} from a trace build")
    parser.add_argument("--out", default=os.path.join(REPO_ROOT, "src", "web", "map-files.txt"))
    parser.add_argument("--models-out", default=os.path.join(REPO_ROOT, "src", "web", "model-files.txt"))
    args = parser.parse_args()

    with open(os.path.join(args.root, "files.txt"), encoding="utf-8") as f:
        listed = [line.strip() for line in f if line.strip()]
    by_lower = {path.lower(): path for path in listed}

    def in_output(path):
        """The output's own spelling of a path the game opened (/game/Data, backslashes, .tga), or None."""
        path = path.replace("\\", "/").lstrip("/")
        path = path[5:] if path.lower().startswith("game/") else path
        stem, suffix = os.path.splitext(path.lower())
        return by_lower.get(stem + STORED_SUFFIX.get(suffix, suffix)) or by_lower.get(path.lower())

    with open(args.trace, encoding="utf-8") as f:
        trace = {int(monster): entry for monster, entry in json.load(f).items()}
    files_of_model = {}
    # The model number of Monster01.bmd: OpenMonsterModel gives Monster<N>.bmd the number N - 1 after it.
    first_monster_model = None
    groups = collections.OrderedDict()
    for monster in sorted(trace):
        model, files = trace[monster][0], trace[monster][1:]
        if files:
            files_of_model.setdefault(model, files)
        opened = []
        for path in files:
            actual = in_output(path)
            if actual and actual not in opened:
                opened.append(actual)
        match = MONSTER_MODEL.fullmatch(opened[0]) if opened else None
        if match and first_monster_model is None:
            first_monster_model = model - (int(match.group(1)) - 1)
        if opened and opened[0].lower().endswith(MODEL_SUFFIX):
            group = groups.setdefault(opened[0], [])
            group.extend(path for path in opened[1:] if path not in group)

    def model_and_textures(model):
        """A monster model the trace has no files of: its file by its number, and its textures."""
        if first_monster_model is None or model < first_monster_model:
            return []
        path = in_output(MONSTER_MODEL_FILE.format(model - first_monster_model + 1))
        if not path:
            return []
        folder = path.rsplit("/", 1)[0]
        names = model_textures.texture_names(os.path.join(args.root, path), make_startup_pack.decrypt_map_file)
        return [path] + [f"{folder}/{name}" for name in names]

    def files_of_monster(monster):
        entry = trace.get(monster)
        if not entry or entry[0] < 0:
            return []
        found = []
        for path in entry[1:] + (files_of_model.get(entry[0]) or model_and_textures(entry[0])):
            actual = in_output(path)
            if actual and actual not in found:
                found.append(actual)
        return found

    packs = collections.defaultdict(list)

    def add(pack, path):
        if path not in packs[pack]:
            packs[pack].append(path)

    with open(os.path.join(REPO_ROOT, MAP_NAMES_SOURCE), encoding="utf-8", errors="replace") as f:
        map_names = MAP_NAME.findall(f.read())
    languages = sorted({path.split("/")[2] for path in listed if path.lower().startswith("data/local/")
                        and path.count("/") > 3})
    for number, name in map_names:
        for language in languages:
            actual = in_output(f"Data/Local/{language}/ImgsMapName/{name}")
            if actual:
                add(pack_of_map(int(number)), actual)
    for path in listed:
        match = MINIMAP.fullmatch(path) or GRADING.fullmatch(path)
        if match:
            add(pack_of_map(int(match.group(1)) - 1), path)

    untraced = set()
    with open(args.spawns, encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            number, monster = (int(part) for part in line.split(","))
            if monster not in trace or trace[monster][0] < 0:
                untraced.add(monster)
            for path in files_of_monster(monster):
                add(pack_of_map(number), path)

    packs_of_file = collections.Counter(path for files in packs.values() for path in files)
    everywhere = sorted(path for path, count in packs_of_file.items() if count > MOST_PACKS_OF_A_FILE)
    lines = ["# Made by tools/web/make_map_files.py: the files a map's pack holds besides the map's own",
             "# folders (its monsters and NPCs, minimap, name, colours). \"<pack's number> <path>\"."]
    total = 0
    for pack in sorted(packs):
        for path in packs[pack]:
            if path not in everywhere:
                lines.append(f"{pack} {path}")
                total += 1
    with open(args.out, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    print(f"{args.out}: {total} lines for {len(packs)} packs")
    lines = ["# Made by tools/web/make_map_files.py: the files the game reads right after a monster's or an",
             "# NPC's model. \"<model> <file>\"."]
    lines.extend(f"{model} {path}" for model, files in groups.items() for path in files)
    with open(args.models_out, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    print(f"{args.models_out}: {len(lines) - 2} lines for {len(groups)} models")
    if untraced:
        print(f"Monsters of the spawns without a trace: {sorted(untraced)}")
    if everywhere:
        print(f"On more than {MOST_PACKS_OF_A_FILE} maps, left out (for the startup or late pack):")
        for path in everywhere:
            print(f"  {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
