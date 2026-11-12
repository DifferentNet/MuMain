r"""Hand-painted HD textures for the MU client: Real-ESRGAN x4, then a Stable Diffusion XL repaint.

hd_textures.py only upscales. This tool repaints the upscale the way an artist would redraw an old
texture at four times the size: SDXL img2img with ControlNet Tile adds fine painted detail (stone
grain, bark, leather, scales, fur, cloth weave), and a post pass keeps the original's colours and
shapes: the final image is the upscale's blurred colours plus the repaint's fine detail, and where
the repaint invented a shape the upscale did not have, the upscale wins.

Stages (each one can be stopped and run again; it goes on where it stopped):

    python -I scripts/hd_art.py plan   --src <client>/Data --work <dir> [--items item-list.txt]
    python -I scripts/hd_art.py esr    --src ... --work ... --esrgan <realesrgan-ncnn-vulkan.exe>
    python -I scripts/hd_art.py sheets --src ... --work ...          (contact sheets for the prompts)
    python -I scripts/hd_art.py paint  --src ... --work ... --sd <sd-cli.exe> --models <dir>
    python -I scripts/hd_art.py write  --src ... --work ... --out <client>/Data/HD
    python -I scripts/hd_art.py cutouts --src ... --work ... --out <client>/Data/HD   (after every write)
    python -I scripts/hd_art.py detail  --src ... --work ... --out <client>/Data/HD   (after write and cutouts)
                                [--only <glob>...] [--dry-run]
    python -I scripts/hd_art.py detail-selftest                     (checks the detail stage on test arrays)

plan     lists every texture of the world (World*, Object*, Monster, NPC, the non-gear Item files,
         Effect, Skill) with its size, alpha and which directions it repeats in.
esr      Real-ESRGAN x4 of each texture (padded by wrapping in the directions it repeats).
sheets   numbered contact sheets of the textures to repaint; a person (or a model) writes
         <work>/prompts/*.json from them: {"Folder/File.OZJ": {"subject": "...", "mode": "paint"}}
         where mode "esr" keeps the plain upscale (glows, flames, particles, light maps).
paint    packs the textures into 1024x1024 canvases (one per 256 px texture, four 128 px ones, ...)
         and repaints each canvas with sd-cli.
write    unpacks the canvases, runs the post pass, joins wrap seams and writes OZJ/OZT files with
         the x4 marker (the format hd_textures.py writes).
cutouts  the leaves in CUTOUTS (and every file with the same source bytes) as ghinzuro picked them
         (2026-11-09, variant C): the x4 alpha made hard and the colours dark, so the Arena's ivy lay on
         the walls as black blots; the same day he had the other darkened leaves, grass and moss done
         too (not monsters, items, metal or stone: there the x4 is only sharper). Their colour goes up to the original's (alpha-weighted mean, per
         channel, at most x CUTOUT_MAX_GAIN), the colour under the edges is filled from the leaves, and
         the alpha is half the x4 one and half the original's smooth one. It works from a copy of the
         written file in <work>/cutouts, so running it again gives the same result. Keep that folder
         with Data/HD: without it the stage takes the already fixed files for clean ones and fixes them
         twice (then put the clean ones back from a published Remaster zip with hd_pack.unpack).
         The leaf litter in CUTOUT_SMOOTH (tree_08, strewn over Lorencia's ground and the Arena's walls)
         gets the original upscaled smoothly instead: its crisp x4 leaves looked stuck in the air
         (ghinzuro 2026-11-10).
detail   the original's fine grain back in the opaque (OZJ) World*, Object*, Monster and NPC textures, after
         write and cutouts (ghinzuro 2026-11-12: Remaster less soapy, stage 4): the repaint smoothed it
         away, so from afar (mip levels) the HD read flat. The detail band is x - gaussian(x, 1.5 px) at the
         original size; where the HD, boxed down, keeps less than DETAIL_TARGET of the original's band,
         the band it lacks is upscaled bicubically and added. Ground tiles wrap both ways, model textures
         wrap where their models repeat them. Skipped: cut-outs, light and reflection maps, World effects on
         black (leaves, fog, map pictures, backdrops), World and Object glows, water, clouds, mist and
         lava (light*, squid*, water, tilewater*, cloud, *mist*, *lava*, wfmoon*, streetlight), OZT files
         (not done yet), files with no original
         or not exactly x4. Like cutouts it keeps the clean file in <work>/detail, so running it again
         gives the same result (a skipped file it changed before gets its clean file back); keep that
         folder with Data/HD.
         --only takes path patterns (fnmatch, any case); --dry-run prints the numbers and writes nothing.

Models: SDXL 1.0 base (CreativeML Open RAIL++-M), the fp16-fix SDXL VAE (MIT),
xinsir controlnet-tile-sdxl-1.0 (Apache-2.0); stable-diffusion.cpp sd-cli (MIT) on Vulkan.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hd_textures as hdt  # noqa: E402

SCALE = hdt.SCALE
CANVAS = 1152  # largest canvas side: a 256 px texture x4 with its margins fits
MARGIN = 32  # canvas pixels of padding around each packed texture
PAINT_FOLDERS = re.compile(r"^(world|object)\d+$|^(monster|npc)$", re.IGNORECASE)
ESR_FOLDERS = re.compile(r"^(effect|skill)$", re.IGNORECASE)

STYLE = ("hand-painted fantasy MMORPG game texture, painterly, crisp fine detail, rich shading, "
         "masterpiece, high detail")
NEG = ("text, letters, watermark, logo, signature, blurry, soft focus, smooth plastic, 3d render, photo, "
       "frame, border, vignette, person, face, jpeg artifacts, low quality")
# Repaint settings picked on test textures (hd-art/test): ControlNet Tile 0.4 keeps the layout of a
# texture atlas, denoise 0.8 lets the model paint new surface detail.
CONTROL = 0.4
DENOISE = 0.8
ATLAS_CONTROL = 0.55
ATLAS_DENOISE = 0.75
STEPS = 20  # 16 steps run at denoise 0.8; 28 looked the same
CFG = 6.0
DETAIL_SIGMA = 4.0  # canvas pixels: one source pixel of a x4 texture
DETAIL_BIAS = 3.0  # how much the repaint's detail is preferred to the upscale's
DETAIL_GAIN = 1.25
TRUST = (0.10, 0.10)  # colour gap where the upscale starts to win, and the width of the fade


# ---------------------------------------------------------------- plan

def model_repeats(src: Path) -> dict:
    """{(folder, texture stem): [repeats left-right, repeats top-bottom]} from the texture coordinates
    of every BMD model: a texture repeats along an axis when a model's coordinates leave 0..1 there."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "hd-models"))
    import bmd
    out = {}
    for path in src.rglob("*.bmd"):
        try:
            model = bmd.read(path)
        except Exception:  # noqa: BLE001 - a few old models use other layouts
            continue
        for mesh in model.meshes:
            if not mesh.uvs:
                continue
            us = [uv[0] for uv in mesh.uvs]
            vs = [uv[1] for uv in mesh.uvs]
            key = (path.parent.name.lower(), mesh.texture.rsplit(".", 1)[0].lower())
            old = out.get(key, [False, False])
            out[key] = [old[0] or min(us) < -0.02 or max(us) > 1.02, old[1] or min(vs) < -0.02 or max(vs) > 1.02]
    return out


def cmd_plan(args):
    items = set()
    if args.items:
        items = {line.strip().lower() for line in args.items.read_text().splitlines() if line.strip()}
    wraps = model_repeats(args.src)
    # --gear: the second pass, character equipment (armour, weapons, wings, mounts): Player, the Item
    # textures the first pass left out, and the model skins in Skill (mounts, summons).
    done = set()
    if args.gear and args.skip_plan:
        done = {e["rel"].lower() for e in json.loads(args.skip_plan.read_text()) if not e["rel"].lower().startswith("skill/")}
    entries = []
    for folder in sorted(args.src.iterdir()):
        if not folder.is_dir():
            continue
        if args.gear:
            paint = folder.name.lower() in ("player", "item", "skill")
            esr = item = False
        else:
            paint = bool(PAINT_FOLDERS.match(folder.name))
            esr = bool(ESR_FOLDERS.match(folder.name))
            item = folder.name.lower() == "item"
        if not (paint or esr or item):
            continue
        for path in sorted(folder.rglob("*")):
            if path.suffix.lower() not in (".ozj", ".ozt") or not path.is_file():
                continue
            if item and path.name.lower() not in items:
                continue  # weapons and armour stay as they are
            if path.relative_to(args.src).as_posix().lower() in done:
                continue
            if path.read_bytes()[:3] == hdt.MARKER[:3]:
                continue
            try:
                image = hdt.read_texture(path)
            except Exception as error:  # noqa: BLE001
                print(f"skip {path}: {error}", file=sys.stderr)
                continue
            rel = path.relative_to(args.src).as_posix()
            wrap_x, wrap_y = wraps.get((path.parent.name.lower(), path.stem.lower()), [False, False])
            if hdt.is_tile(Path(rel)):
                wrap_x = wrap_y = True
            # Reflection maps (_R) are blurred on purpose; tiny textures have nothing to repaint.
            mode = "paint" if (paint or item) and not re.search(r"_r$", path.stem, re.I) \
                and min(image.size) >= 16 else "esr"
            entries.append({"rel": rel, "w": image.size[0], "h": image.size[1], "alpha": image.mode == "RGBA",
                            "wrap": [wrap_x, wrap_y], "mode": mode})
    args.work.mkdir(parents=True, exist_ok=True)
    (args.work / "plan.json").write_text(json.dumps(entries, indent=0))
    n_paint = sum(e["mode"] == "paint" for e in entries)
    print(f"{len(entries)} textures, {n_paint} to repaint, {len(entries) - n_paint} upscale only")


def load_plan(work: Path):
    return json.loads((work / "plan.json").read_text())


# ---------------------------------------------------------------- esr

def pad_axes(image: Image.Image, amount: int, wrap) -> Image.Image:
    """Pads by wrapping along the axes that repeat and by repeating the edge along the others."""
    wrapped = hdt.pad(image, amount, True)
    edged = hdt.pad(image, amount, False)
    if wrap[0] and wrap[1]:
        return wrapped
    if not wrap[0] and not wrap[1]:
        return edged
    w, h = image.size
    out = edged.copy()
    if wrap[0]:  # take the left and right margins from the wrapped copy
        out.paste(wrapped.crop((0, amount, amount, amount + h)), (0, amount))
        out.paste(wrapped.crop((amount + w, amount, 2 * amount + w, amount + h)), (amount + w, amount))
    else:
        out.paste(wrapped.crop((amount, 0, amount + w, amount)), (amount, 0))
        out.paste(wrapped.crop((amount, amount + h, amount + w, 2 * amount + h)), (amount, amount + h))
    return out


def esr_path(work: Path, rel: str) -> Path:
    return work / "esr" / (rel + ".png")


def cmd_esr(args):
    import shutil
    import tempfile
    plan = load_plan(args.work)
    todo = [e for e in plan if not esr_path(args.work, e["rel"]).exists()]
    print(f"{len(todo)} of {len(plan)} to upscale")
    pad_src = MARGIN // SCALE
    for start in range(0, len(todo), 200):
        batch = todo[start:start + 200]
        tmp = Path(tempfile.mkdtemp(prefix="hdart-"))
        try:
            (tmp / "in").mkdir()
            (tmp / "out").mkdir()
            for i, e in enumerate(batch):
                image = hdt.read_texture(args.src / e["rel"])
                padded = pad_axes(image, pad_src, e["wrap"])
                padded.convert("RGB").save(tmp / "in" / f"{i:05d}_rgb.png")
                if e["alpha"]:
                    a = padded.getchannel("A")
                    Image.merge("RGB", (a, a, a)).save(tmp / "in" / f"{i:05d}_a.png")
            subprocess.run([str(args.esrgan), "-i", str(tmp / "in"), "-o", str(tmp / "out"), "-n",
                            "realesrgan-x4plus", "-s", "4", "-m", str(args.esrgan.parent / "models"), "-f", "png"],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for i, e in enumerate(batch):
                # Kept with the margin: the paint stage needs it.
                rgb = Image.open(tmp / "out" / f"{i:05d}_rgb.png").convert("RGB")
                if e["alpha"]:
                    rgb.putalpha(Image.open(tmp / "out" / f"{i:05d}_a.png").convert("L"))
                target = esr_path(args.work, e["rel"])
                target.parent.mkdir(parents=True, exist_ok=True)
                rgb.save(target)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        print(f"upscaled {min(start + 200, len(todo))}/{len(todo)}", flush=True)


# ---------------------------------------------------------------- sheets

def cmd_sheets(args):
    plan = [e for e in load_plan(args.work) if e["mode"] == "paint"]
    out = args.work / "sheets"
    out.mkdir(parents=True, exist_ok=True)
    cell, cols, per = 192, 6, 30
    index = []
    for s in range(0, len(plan), per):
        chunk = plan[s:s + per]
        rows = (len(chunk) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * cell, rows * (cell + 18)), (40, 40, 40))
        draw = ImageDraw.Draw(sheet)
        for i, e in enumerate(chunk):
            image = hdt.read_texture(args.src / e["rel"])
            if image.mode == "RGBA":  # show cut-outs on a checker so their shape reads
                bg = Image.new("RGBA", image.size, (90, 90, 90, 255))
                bg.alpha_composite(image)
                image = bg
            image = image.convert("RGB")
            image.thumbnail((cell - 4, cell - 4), Image.LANCZOS)
            x, y = (i % cols) * cell, (i // cols) * (cell + 18)
            sheet.paste(image, (x + 2, y + 2))
            draw.text((x + 3, y + cell), f"{s + i}", fill=(255, 230, 120))
            index.append({"n": s + i, "rel": e["rel"], "size": f"{e['w']}x{e['h']}", "alpha": e["alpha"]})
        sheet.save(out / f"sheet_{s // per:04d}.png")
    (out / "index.json").write_text(json.dumps(index, indent=0))
    print(f"{(len(plan) + per - 1) // per} sheets for {len(plan)} textures in {out}")


# ---------------------------------------------------------------- paint

def load_prompts(work: Path) -> dict:
    prompts = {}
    for f in sorted((work / "prompts").glob("*.json")):
        prompts.update(json.loads(f.read_text(encoding="utf-8")))
    return prompts


def fill_transparent(rgba: Image.Image) -> Image.Image:
    """RGB of a cut-out with the transparent parts filled by the nearby opaque colours, so the
    repaint does not pull black fringes into leaves and fences."""
    a = np.asarray(rgba, dtype=np.float32) / 255
    rgb, alpha = a[..., :3], a[..., 3:4]
    if alpha.min() > 0.5:
        return rgba.convert("RGB")
    w = (alpha > 0.5).astype(np.float32)
    levels = []
    c, m = rgb * w, w
    while min(c.shape[:2]) > 1:
        levels.append((c, m))
        h2, w2 = c.shape[0] // 2 * 2, c.shape[1] // 2 * 2
        c = c[:h2, :w2].reshape(h2 // 2, 2, w2 // 2, 2, 3).sum((1, 3))
        m = m[:h2, :w2].reshape(h2 // 2, 2, w2 // 2, 2, 1).sum((1, 3))
    filled = c / np.maximum(m, 1e-6)
    for c, m in reversed(levels):
        up = np.asarray(Image.fromarray((np.clip(filled, 0, 1) * 255).astype(np.uint8)).resize(
            (c.shape[1], c.shape[0]), Image.BILINEAR), dtype=np.float32) / 255
        own = c / np.maximum(m, 1e-6)
        k = np.clip(m, 0, 1)
        filled = own * k + up * (1 - k)
    out = rgb * w + filled * (1 - w)
    return Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")


def pack(entries):
    """Shelf-packs padded x4 textures into canvases of at most CANVAS px a side. Textures bigger than
    that are cut into overlapping pieces, each its own canvas. Returns canvases
    {"w", "h", "items": [{"rel", "x", "y", "w", "h", "cx", "cy"}]}: (x, y, w, h) is the piece's place on
    the canvas, (cx, cy) its offset inside the padded upscale."""
    canvases, small = [], []
    for e in entries:
        w, h = e["w"] * SCALE + 2 * MARGIN, e["h"] * SCALE + 2 * MARGIN
        if w <= CANVAS and h <= CANVAS:
            small.append((e, w, h))
            continue
        step = CANVAS - 2 * MARGIN
        for cy in range(0, max(1, h - 2 * MARGIN), step):
            for cx in range(0, max(1, w - 2 * MARGIN), step):
                pw, ph = min(CANVAS, w - cx), min(CANVAS, h - cy)
                canvases.append({"items": [{"rel": e["rel"], "x": 0, "y": 0, "w": pw, "h": ph,
                                            "cx": cx, "cy": cy}]})
    small.sort(key=lambda t: (-t[2], -t[1], t[0]["rel"]))
    canvas, shelf_y, shelf_h, x = None, 0, 0, 0
    for e, w, h in small:
        if canvas is not None and x + w <= CANVAS and h <= shelf_h:
            pass  # fits on the current shelf
        elif canvas is not None and shelf_y + shelf_h + h <= CANVAS:
            shelf_y, shelf_h, x = shelf_y + shelf_h, h, 0
        else:
            canvas = {"items": []}
            canvases.append(canvas)
            shelf_y, shelf_h, x = 0, h, 0
        canvas["items"].append({"rel": e["rel"], "x": x, "y": shelf_y, "w": w, "h": h, "cx": 0, "cy": 0})
        x += w
    for c in canvases:  # SDXL wants sides in steps of 64
        c["w"] = -(-max(i["x"] + i["w"] for i in c["items"]) // 64) * 64
        c["h"] = -(-max(i["y"] + i["h"] for i in c["items"]) // 64) * 64
    return canvases


def group_key(e, prompts):
    folder = e["rel"].split("/")[0].lower()
    kind = prompts.get(e["rel"], {}).get("kind", "")
    return (re.sub(r"\d+$", "", folder), kind)


def canvas_prompt(items, prompts):
    subjects = []
    for it in items:
        s = prompts.get(it["rel"], {}).get("subject", "")
        if s and s not in subjects:
            subjects.append(s)
    return "; ".join(subjects[:4])


def build_jobs(work: Path, src: Path):
    plan = {e["rel"]: e for e in load_plan(work)}
    prompts = load_prompts(work)
    paint = [e for e in plan.values() if e["mode"] == "paint" and prompts.get(e["rel"], {}).get("mode", "paint") == "paint"]
    # Many maps share the same files under other folder names: paint each picture once.
    first, same = {}, {}
    for e in sorted(paint, key=lambda e: e["rel"]):
        key = (hashlib.md5((src / e["rel"]).read_bytes()).hexdigest(), tuple(e["wrap"]))
        same[e["rel"]] = first.setdefault(key, e["rel"])
    (work / "same.json").write_text(json.dumps(same, indent=0))
    paint = [e for e in paint if same[e["rel"]] == e["rel"]]
    groups = {}
    for e in paint:
        groups.setdefault(group_key(e, prompts), []).append(e)
    jobs = []
    order = {"monster": 0, "npc": 1, "object": 2, "item": 3, "world": 4}  # what players look at most first
    for key in sorted(groups, key=lambda k: (order.get(k[0], 9), k)):
        for canvas in pack(sorted(groups[key], key=lambda e: e["rel"])):
            canvas["prompt"] = canvas_prompt(canvas["items"], prompts)
            # Unwrapped models (faces, emblems) get less freedom than plain surfaces: at 0.4/0.8
            # the repaint turned an orc's face into a helmet; 0.55/0.75 keeps it.
            canvas["control"], canvas["denoise"] = (ATLAS_CONTROL, ATLAS_DENOISE) if key[1] == "atlas" else (CONTROL, DENOISE)
            tag = json.dumps(canvas["items"], sort_keys=True)
            if (canvas["control"], canvas["denoise"]) != (CONTROL, DENOISE):
                tag += f"|{canvas['control']}|{canvas['denoise']}"
            canvas["id"] = f"{zlib.crc32(tag.encode()):08x}"
            jobs.append(canvas)
    return jobs


def compose(work: Path, canvas) -> Image.Image:
    plan = {e["rel"]: e for e in load_plan(work)}
    out = Image.new("RGB", (canvas["w"], canvas["h"]), (128, 128, 128))
    for it in canvas["items"]:
        up = Image.open(esr_path(work, it["rel"]))
        up = fill_transparent(up) if up.mode == "RGBA" else up.convert("RGB")
        out.paste(up.crop((it["cx"], it["cy"], it["cx"] + it["w"], it["cy"] + it["h"])), (it["x"], it["y"]))
    return out


def cmd_paint(args):
    jobs = build_jobs(args.work, args.src)
    (args.work / "jobs.json").write_text(json.dumps(jobs, indent=0))
    folder = args.work / "canvas"
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, VK_LOADER_LAYERS_DISABLE="~implicit~")
    todo = [j for j in jobs if not (folder / f"{j['id']}.out.png").exists()]
    if args.limit:
        todo = todo[:args.limit]
    print(f"{len(todo)} of {len(jobs)} canvases to paint", flush=True)
    started = time.time()
    for n, job in enumerate(todo, 1):
        src = folder / f"{job['id']}.in.png"
        compose(args.work, job).save(src)
        dst = folder / f"{job['id']}.out.png"
        seed = zlib.crc32(job["id"].encode()) & 0x7FFFFFFF
        prompt = f"{job['prompt']}, {STYLE}" if job["prompt"] else STYLE
        cmd = [str(args.sd), "-m", str(args.models / "sd_xl_base_1.0.safetensors"),
               "--vae", str(args.models / "sdxl.vae.safetensors"),
               "--control-net", str(args.models / "controlnet-tile-sdxl.safetensors"), "--control-image", str(src),
               "--control-strength", str(job["control"]), "--conv-direct", "auto,vae=on,controlnet=on,diffusion=on",
               # whole-canvas VAE decode above 1024 px runs out of memory and crashes sd-cli
               "--vae-tiling", "--vae-tile-size", "576",
               "-M", "img_gen", "-i", str(src), "--strength", str(job["denoise"]), "--steps", str(STEPS),
               "--cfg-scale", str(CFG), "--sampling-method", "dpm++2m", "--scheduler", "karras",
               "-W", str(job["w"]), "-H", str(job["h"]), "--seed", str(seed), "-p", prompt, "-n", NEG,
               "-o", str(dst.with_suffix(".tmp.png"))]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if r.returncode or not dst.with_suffix(".tmp.png").exists():
            print(f"FAILED {job['id']} ({r.returncode}): {r.stderr[-400:]}", flush=True)
            continue
        dst.with_suffix(".tmp.png").replace(dst)
        left = (time.time() - started) / n * (len(todo) - n)
        print(f"{n}/{len(todo)} {job['id']} {len(job['items'])} tex, ~{left / 3600:.1f} h left", flush=True)


# ---------------------------------------------------------------- write

CUTOUTS = (
    # the Arena's ivy and its bushes (2026-11-09, variant C)
    "Object7/tree_01.OZT", "Object7/tree_02.OZT", "Object7/tree_08.OZT",
    # the other leaves, grass and moss 15-43 % darker than the original ("только листва", same day)
    "Object1/tree_06.OZT", "Object1/Tree_a.OZT",
    "Object32/treeleap.OZT",
    "Object34/Atree011.OZT", "Object34/Pool004.OZT", "Object34/Pool004_org.OZT",
    "Object35/cr1_branch01.OZT", "Object35/cr1_branch02.OZT", "Object35/cr1_branch04.OZT",
    "Object35/cr1_branch06.OZT", "Object35/leaves.OZT",
    "Object38/han_moss4.OZT", "Object38/han_moss5.OZT", "Object38/han_moss6.OZT", "Object38/Pool001.OZT",
    "Object41/csr_wood.OZT", "Object41/csr_wood1.OZT", "Object41/cho_tree_d.OZT", "Object41/tree_a1.OZT",
    "Object42/cryleaftypeA.OZT", "Object42/cryleaftypeC.OZT", "Object42/cr1_grass02.OZT",
    "object55/severleaf01.ozt", "object56/severleaf01.ozt",
    "Object64/so_tree05.OZT", "Object81/so_ktree01.OZT",
    "World34/TileGrass01.OZT", "World38/TileGrass01.OZT",
)
# Leaf litter strewn flat over ground, stones and walls (Lorencia Grass01/02, the Arena's Object32/33):
# the x4 makes crisp dark single leaves that look stuck in the air over the surface ("плавают в воздухе",
# ghinzuro 2026-11-10, sun shadows off made no difference). These take the original upscaled smoothly,
# the Classic look.
CUTOUT_SMOOTH = ("Object7/tree_08.OZT",)
CUTOUT_MAX_GAIN = 1.6
CUTOUT_ALPHA_MIX = 0.5  # share of the x4 alpha; the rest is the original's, upscaled smoothly


def smooth_upscale(rgba: np.ndarray, size) -> np.ndarray:
    """Bicubic upscale of a straight-alpha float RGBA, through premultiplied colour (no dark fringes)."""
    pm = np.dstack([rgba[..., :3] * rgba[..., 3:], rgba[..., 3:]]).astype(np.float32)
    big = np.stack([np.asarray(Image.fromarray(pm[..., c], "F").resize(size, Image.BICUBIC)) for c in range(4)], -1)
    big = np.clip(big, 0, 1)
    alpha = big[..., 3:]
    return np.dstack([np.where(alpha > 1e-3, big[..., :3] / np.maximum(alpha, 1e-3), 0).clip(0, 1), alpha])


def fix_cutout(original: Image.Image, hd: Image.Image) -> Image.Image:
    """Variant C of a cut-out: the original's brightness, clean edge colour, half-soft alpha."""
    o = np.asarray(original.convert("RGBA"), dtype=np.float64) / 255
    h = np.asarray(hd.convert("RGBA"), dtype=np.float64) / 255
    ou = smooth_upscale(o, hd.size)

    def weighted_mean(a):
        return (a[..., :3] * a[..., 3:]).sum((0, 1)) / max(a[..., 3].sum(), 1e-6)

    gain = np.clip(weighted_mean(ou) / np.maximum(weighted_mean(h), 1e-3), 1, CUTOUT_MAX_GAIN)
    filled = np.asarray(fill_transparent(hd.convert("RGBA")), dtype=np.float64) / 255
    rgb = np.clip(filled * gain, 0, 1)
    alpha = np.clip(CUTOUT_ALPHA_MIX * h[..., 3] + (1 - CUTOUT_ALPHA_MIX) * ou[..., 3], 0, 1)
    out = np.dstack([rgb, alpha])
    return Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA")


def smooth_cutout(original: Image.Image, hd: Image.Image) -> Image.Image:
    """The original upscaled smoothly to the HD size: no x4 detail at all (CUTOUT_SMOOTH)."""
    o = np.asarray(original.convert("RGBA"), dtype=np.float64) / 255
    return Image.fromarray((smooth_upscale(o, hd.size) * 255 + 0.5).astype(np.uint8), "RGBA")


def cmd_cutouts(args):
    wanted = {(args.src / rel).read_bytes(): rel for rel in CUTOUTS}
    rels = []
    for path in sorted(args.src.rglob("*.OZT")) + sorted(args.src.rglob("*.ozt")):
        rel = path.relative_to(args.src).as_posix()
        if rel.split("/")[0].upper() != "HD" and path.read_bytes() in wanted and (args.out / rel).exists():
            rels.append(rel)
    smooth = {(args.src / rel).read_bytes() for rel in CUTOUT_SMOOTH}
    for rel in sorted(set(rels)):
        keep = args.work / "cutouts" / rel
        last = keep.with_name(keep.name + ".written.md5")  # what this stage wrote last time
        current = (args.out / rel).read_bytes()
        if not keep.exists() or not last.exists() or last.read_text() != hashlib.md5(current).hexdigest():
            # first run, or write put a new texture there since: keep that one
            keep.parent.mkdir(parents=True, exist_ok=True)
            keep.write_bytes(current)
        # the note first, then the file: a run stopped in between finds a file that differs from the
        # note, which is still the clean one, and keeps it again
        tmp = args.out / (rel + ".tmp")
        fix = smooth_cutout if (args.src / rel).read_bytes() in smooth else fix_cutout
        hdt.write_ozt(tmp, fix(hdt.read_texture(args.src / rel), hdt.read_texture(keep)))
        last.write_text(hashlib.md5(tmp.read_bytes()).hexdigest())
        os.replace(tmp, args.out / rel)
        print("cut-out", rel, "(smooth)" if fix is smooth_cutout else "")
    print(f"{len(set(rels))} cut-outs written to {args.out}")


def gblur(a, sigma, wrap=(False, False)):
    """Gaussian blur by FFT; circular along the axes in wrap, mirrored along the others."""
    h, w = a.shape[:2]
    p = int(sigma * 3) + 1
    py, px = (0 if wrap[1] else p), (0 if wrap[0] else p)
    if py or px:
        a = np.pad(a, ((py, py), (px, px), (0, 0)), mode="reflect")
    H, W = a.shape[:2]
    fy = np.fft.fftfreq(H)[:, None]
    fx = np.fft.rfftfreq(W)[None, :]
    k = np.exp(-2 * (np.pi * sigma) ** 2 * (fx ** 2 + fy ** 2))
    out = np.fft.irfft2(np.fft.rfft2(a, axes=(0, 1)) * k[..., None], s=(H, W), axes=(0, 1))
    return out[py:py + h, px:px + w]


def lock(painted, up, sigma=DETAIL_SIGMA, trust=TRUST):
    """The upscale's colours and shapes with the repaint's fine detail. Fine detail is mixed by its
    strength: where the upscale has crisp lines of its own (outlines, glowing eyes, trims) they stay,
    where it is smooth and plastic the repaint's texture comes in, a little stronger than painted."""
    gp, gu = gblur(painted, sigma), gblur(up, sigma)
    dp, du = painted - gp, up - gu
    ep = gblur((dp ** 2).mean(2, keepdims=True), sigma * 2)
    eu = gblur((du ** 2).mean(2, keepdims=True), sigma * 2)
    keep = eu / (eu + DETAIL_BIAS * ep + 1e-6)
    out = gu + keep * du + (1 - keep) * DETAIL_GAIN * dp
    if trust:
        big = sigma * 3
        d = np.abs(gblur(painted, big) - gblur(up, big)).max(axis=2, keepdims=True)
        m = gblur(np.clip((d - trust[0]) / trust[1], 0, 1), big)
        out = out * (1 - m) + up * m
    return np.clip(out, 0, 1)


def join_seams(full, w, h, wrap):
    """full: the post-passed padded texture (h + 2M, w + 2M). Near each edge that repeats, blends the
    texture with the copy of the same texels painted in the opposite margin, half and half right at
    the edge, so the two sides meet exactly."""
    M = MARGIN
    core = full[M:M + h, M:M + w].copy()
    if wrap[0]:
        t = np.clip(np.arange(M) / M, 0, 1)[None, :, None] * 0.5 + 0.5  # 0.5 at the edge -> 1 inside
        left_copy = full[M:M + h, M + w:M + w + M]  # texels 0..M-1 painted after the right edge
        right_copy = full[M:M + h, 0:M]  # texels w-M..w-1 painted before the left edge
        core[:, :M] = core[:, :M] * t + left_copy * (1 - t)
        core[:, w - M:] = core[:, w - M:] * t[:, ::-1] + right_copy * (1 - t[:, ::-1])
    if wrap[1]:
        t = np.clip(np.arange(M) / M, 0, 1)[:, None, None] * 0.5 + 0.5
        top_copy = full[M + h:M + h + M, M:M + w]
        bottom_copy = full[0:M, M:M + w]
        core[:M] = core[:M] * t + top_copy * (1 - t)
        core[h - M:] = core[h - M:] * t[::-1] + bottom_copy * (1 - t[::-1])
    return core


# ---------------------------------------------------------------- effects, glows, sky
# Not repainted: variant B (a clean glow pass over the ESR upscale) for crisp effects, variant E (bicubic
# upscale of the original with the same pass) for soft ones, where ESR turns soft blobs into hard shapes
# and makes animation frames flicker. Chosen by ghinzuro 2026-11-08.
FX_SOFT_EDGES = 0.13  # strongest source edges below this: soft (clouds, mist, light shafts)
FX_ESR_DRIFT = 0.12  # ESR differs from the source by more than this (relative to contrast): soft


FX_SOFT_WORDS = re.compile(r"cloud|mist|fog|smoke|steam|caustic|glow|light|sky|aurora|haze", re.I)


def fx_soft(e, src, up_img, subject="") -> bool:
    """Soft glows, clouds and caustics get E. Model skins and stone that sit in the effect folders keep
    B even when blurry: E would only blur them more. So E needs a dark (additive) background or a
    caption that says it is light, cloud or water."""
    w, h = e["w"], e["h"]
    a = np.asarray(hdt.read_texture(src / e["rel"]).convert("L"), dtype=np.float64) / 255
    if not (np.percentile(a, 20) < 0.06 or FX_SOFT_WORDS.search(subject)):
        return False
    edges = np.abs(np.diff(a, axis=0))[:, :-1] + np.abs(np.diff(a, axis=1))[:-1]
    if np.percentile(edges, 99) < FX_SOFT_EDGES:
        return True
    M = MARGIN
    down = up_img.convert("L").crop((M, M, M + w * SCALE, M + h * SCALE)).resize((w, h), Image.BOX)
    drift = np.abs(np.asarray(down, dtype=np.float64) / 255 - a).mean() / (a.std() + 0.02)
    return drift > FX_ESR_DRIFT


def fx_clean(a, w, additive, wrap):
    """a: padded float RGB at x4. Smooths flat areas (JPEG blocks, banding) but keeps edges, crisper
    cores, a soft bloom and a little more colour; additive textures get a true black floor so JPEG
    noise does not show as grey boxes in game."""
    n = w * SCALE / 256  # blur sizes as tuned on the samples, where a 64 px source drawn at 256 px is 1
    sm = gblur(a, 1.5 * n, wrap)
    det = gblur(np.abs(a - sm).max(axis=2, keepdims=True), 3 * n, wrap)
    k = np.clip(det / 0.04, 0, 1)
    a = sm * (1 - k) + a * k
    base = gblur(a, 6 * n, wrap)
    a = base + 1.25 * (a - base)
    if additive:
        a = a + 0.35 * gblur(np.clip(a - 0.55, 0, 1), 10 * n, wrap)
    lum = a.mean(axis=2, keepdims=True)
    a = np.clip(lum + 1.15 * (a - lum), 0, 1)
    if additive:
        a = a * np.clip(a / 0.03, 0, 1) ** 2
    return a


def fx_texture(e, src, up_img, subject=""):
    """Returns (rgb Image, alpha Image or None, soft) for an effect texture, cropped to x4 size."""
    w, h, M = e["w"], e["h"], MARGIN
    soft = fx_soft(e, src, up_img, subject)
    if soft:
        padded = pad_axes(hdt.read_texture(src / e["rel"]), M // SCALE, e["wrap"])
        big = padded.resize((padded.width * SCALE, padded.height * SCALE), Image.BICUBIC)
    else:
        big = up_img
    alpha = big.getchannel("A") if big.mode == "RGBA" else None
    rgb = np.asarray(fill_transparent(big) if alpha else big.convert("RGB"), dtype=np.float64) / 255
    # the margin already holds the wrapped or edge-repeated neighbours, so blur it plainly
    out = fx_clean(rgb, min(w, h), alpha is None, (False, False))
    box = (M, M, M + w * SCALE, M + h * SCALE)
    img = Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGB").crop(box)
    return img, (alpha.crop(box) if alpha else None), soft


def fx_wanted(e) -> bool:
    # terrain light maps set the lighting of a map: a glow pass would change it
    return min(e["w"], e["h"]) >= 16 and "terrainlight" not in e["rel"].lower()


def cmd_write(args):
    plan = {e["rel"]: e for e in load_plan(args.work)}
    jobs = json.loads((args.work / "jobs.json").read_text()) if (args.work / "jobs.json").exists() else []
    pieces = {}
    for job in jobs:
        out = args.work / "canvas" / f"{job['id']}.out.png"
        if not out.exists():
            continue
        for it in job["items"]:
            pieces.setdefault(it["rel"], []).append((out, it))
    same_path = args.work / "same.json"
    same = json.loads(same_path.read_text()) if same_path.exists() else {}
    for rel, canon in same.items():
        if canon != rel and canon in pieces:
            pieces[rel] = pieces[canon]
    written = 0
    fx_count = {False: 0, True: 0}
    prompts = load_prompts(args.work)
    for rel, e in plan.items():
        target = args.out / rel
        if target.exists() and not args.force:
            continue
        if args.only and not any(Path(rel).match(p) for p in args.only):
            continue
        up_img = Image.open(esr_path(args.work, rel))
        alpha = up_img.getchannel("A") if up_img.mode == "RGBA" else None
        if alpha:
            alpha_out = alpha.crop((MARGIN, MARGIN, MARGIN + e["w"] * SCALE, MARGIN + e["h"] * SCALE))
        up = np.asarray(fill_transparent(up_img) if alpha else up_img.convert("RGB"), dtype=np.float64) / 255
        H, W = up.shape[:2]
        if rel in pieces:
            painted = np.zeros_like(up)
            weight = np.zeros((H, W, 1))
            for out, it in pieces[rel]:
                c = np.asarray(Image.open(out).convert("RGB"), dtype=np.float64)[it["y"]:it["y"] + it["h"],
                                                                                  it["x"]:it["x"] + it["w"]] / 255
                # feathered weight so overlapping pieces of big textures blend
                ramp_y = np.minimum(np.arange(it["h"]) + 1, it["h"] - np.arange(it["h"]))[:, None]
                ramp_x = np.minimum(np.arange(it["w"]) + 1, it["w"] - np.arange(it["w"]))[None, :]
                wgt = np.clip(np.minimum(ramp_y, ramp_x) / MARGIN, 1e-3, 1)[..., None]
                painted[it["cy"]:it["cy"] + it["h"], it["cx"]:it["cx"] + it["w"]] += c * wgt
                weight[it["cy"]:it["cy"] + it["h"], it["cx"]:it["cx"] + it["w"]] += wgt
            if (weight == 0).any():
                print(f"{rel}: not all pieces painted yet", file=sys.stderr)
                continue
            painted /= weight
            full = lock(painted, up)
            rgb = join_seams(full, e["w"] * SCALE, e["h"] * SCALE, e["wrap"])
            rgb_img = Image.fromarray((rgb * 255 + 0.5).astype(np.uint8), "RGB")
        else:
            if e["mode"] == "paint" and not args.allow_unpainted:
                if prompts.get(rel, {}).get("mode", "paint") == "paint":
                    continue  # waits for its canvas
            M = MARGIN
            if fx_wanted(e):
                rgb_img, fx_alpha, soft = fx_texture(e, args.src, up_img, prompts.get(rel, {}).get("subject", ""))
                fx_count[soft] += 1
                if fx_alpha is not None:
                    alpha_out = fx_alpha
            else:
                rgb_img = up_img.convert("RGB").crop((M, M, M + e["w"] * SCALE, M + e["h"] * SCALE))
                alpha_c = alpha.crop((M, M, M + e["w"] * SCALE, M + e["h"] * SCALE)) if alpha else None
                seed = zlib.crc32(rel.lower().encode())
                rgb_img = hdt.finish(hdt.read_texture(args.src / rel), rgb_img, alpha_c, seed)
        if alpha:
            rgba = rgb_img.copy()
            rgba.putalpha(alpha_out)
            target.parent.mkdir(parents=True, exist_ok=True)
            hdt.write_ozt(target, rgba)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            hdt.write_ozj(target, rgb_img)
        written += 1
        if args.png:
            p = args.work / "preview" / (rel + ".png")
            p.parent.mkdir(parents=True, exist_ok=True)
            rgb_img.save(p)
    print(f"wrote {written} textures to {args.out}; effects: {fx_count[False]} crisp (B), {fx_count[True]} soft (E)")


# ---------------------------------------------------------------- detail
# The repaint smoothed the original's fine grain away: box-downsampled to the original size, the HD
# World1 ground kept ~65 % (worst 33 %) of the original's detail band, so at a distance (mip levels)
# the Remaster read flat. This stage adds back the part of that band the HD lacks.
DETAIL_TARGET = 0.85  # share of the original's detail band the HD should keep at the original size
DETAIL_SIGMA_SRC = 1.5  # source pixels: the detail band is x - gaussian(x, this)
DETAIL_MAX_GAIN = 1.0  # at most the whole missing band is added
DETAIL_MIN_ENERGY = 0.004  # mean |band| of the original below this: flat, nothing to bring back
# Light maps set a map's lighting and reflection maps (_R) are blurred on purpose: no grain there.
DETAIL_SKIP = re.compile(r"terrainlight|_r$", re.IGNORECASE)
# World* textures drawn like effects on black (falling leaves, fog, the map picture, backdrops).
DETAIL_SKIP_WORLD = re.compile(r"^(leaf\d*|fog\d*|map\d*|bg_b_.*)$", re.IGNORECASE)
# World* and Object* glows, water, clouds, mist and lava: grain would only add noise there.
DETAIL_SKIP_EFFECT = re.compile(r"^(light\d*|squid\d*|water|cswater|tilewater\d*(?:_org)?|cloud|.*mist.*|.*lava.*|wfmoon.*"
                                r"|streetlight)$", re.IGNORECASE)
_detail_replace = os.replace  # the self-check swaps it to stop a run between the note and the file


def box_down(a: np.ndarray, factor: int = SCALE) -> np.ndarray:
    """Mean of each factor x factor block (an exact box downsample)."""
    h, w = a.shape[0] // factor, a.shape[1] // factor
    return a[:h * factor, :w * factor].reshape(h, factor, w, factor, -1).mean((1, 3))


def upscale_band(a: np.ndarray, size, wrap) -> np.ndarray:
    """Bicubic x4 of a float (h, w, c) array to size (W, H), padded by wrapping along the axes in wrap
    and by repeating the edge along the others, so a repeating texture stays seamless."""
    p = 2
    a = np.pad(a, ((0, 0), (p, p), (0, 0)), mode="wrap" if wrap[0] else "edge")
    a = np.pad(a, ((p, p), (0, 0), (0, 0)), mode="wrap" if wrap[1] else "edge")
    h, w = a.shape[:2]
    big = np.stack([np.asarray(Image.fromarray(np.ascontiguousarray(a[..., c], dtype=np.float32), "F").resize(
        (w * SCALE, h * SCALE), Image.BICUBIC)) for c in range(a.shape[2])], -1)
    return big[p * SCALE:p * SCALE + size[1], p * SCALE:p * SCALE + size[0]]


def detail_ratio(o: np.ndarray, hd: np.ndarray, wrap):
    """(ratio, band_o, band_h, energy_o): how much of the original's detail band the HD keeps at the
    original size. o and hd are float RGB in 0..1, hd exactly SCALE times larger."""
    down = box_down(hd)
    band_o = o - gblur(o, DETAIL_SIGMA_SRC, wrap)
    band_h = down - gblur(down, DETAIL_SIGMA_SRC, wrap)
    energy_o = float(np.abs(band_o).mean())
    return float(np.abs(band_h).mean()) / (energy_o + 1e-6), band_o, band_h, energy_o


def add_detail(o: np.ndarray, hd: np.ndarray, wrap):
    """Returns (new hd or None if left as is, ratio before, k). Adds k of the band the HD lacks,
    upscaled bicubically, so the ratio comes up to about DETAIL_TARGET."""
    ratio, band_o, band_h, energy_o = detail_ratio(o, hd, wrap)
    if ratio >= DETAIL_TARGET or energy_o < DETAIL_MIN_ENERGY:
        return None, ratio, 0.0
    k = min(DETAIL_MAX_GAIN, (DETAIL_TARGET - ratio) / max(1 - ratio, 1e-3))
    add = upscale_band(band_o - band_h, (hd.shape[1], hd.shape[0]), wrap) * k
    return np.clip(hd + add, 0, 1), ratio, k


def detail_wrap(rel: str, repeats: dict):
    if hdt.is_tile(Path(rel)):
        return (True, True)
    p = Path(rel)
    return tuple(repeats.get((p.parent.name.lower(), p.stem.lower()), (False, False)))


def detail_skip_reason(rel: str):
    stem = Path(rel).stem
    if DETAIL_SKIP.search(stem):
        return "light or reflection map"
    if rel.split("/")[0].lower().startswith("world") and DETAIL_SKIP_WORLD.match(stem):
        return "effect on black"
    if re.match(r"^(world|object)", rel.split("/")[0], re.IGNORECASE) and DETAIL_SKIP_EFFECT.match(stem):
        return "effect / water"
    return None


def cmd_detail(args):
    import fnmatch
    started = time.time()
    skip_bytes = {(args.src / rel).read_bytes() for rel in CUTOUTS + CUTOUT_SMOOTH if (args.src / rel).exists()}
    skip_rels = {rel.lower() for rel in CUTOUTS + CUTOUT_SMOOTH}
    rels, stale = [], []
    for path in sorted(args.out.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(args.out).as_posix()
        if not PAINT_FOLDERS.match(rel.split("/")[0]):
            continue  # Item, Player, Effect, Skill, Interface and the rest keep their HD as is
        if path.name.lower().endswith(".ozj.tmp"):
            stale.append(path)  # left by a run of this stage that was stopped
            continue
        if path.suffix.lower() != ".ozj":
            continue
        if args.only and not any(fnmatch.fnmatch(rel.lower(), p.lower()) for p in args.only):
            continue
        rels.append(rel)
    if not args.dry_run:
        for path in stale:
            os.remove(path)
            print(f"removed stale {path.relative_to(args.out).as_posix()}")
    # which directions each model texture repeats in, read only from the folders taken
    repeats = {}
    for folder in sorted({rel.split("/")[0] for rel in rels}):
        if (args.src / folder).is_dir():
            repeats.update(model_repeats(args.src / folder))
    before, after = [], []
    count = {"changed": 0, "written": 0, "restored": 0, "skipped": 0}
    for rel in rels:
        src = args.src / rel
        target = args.out / rel
        keep = args.work / "detail" / rel
        # md5s of the file in --out before the last write and of what it wrote: the file is this
        # stage's own when it matches either (a run stopped between the note and the file left the old)
        last = keep.with_name(keep.name + ".written.md5")
        current = target.read_bytes()
        ours = keep.exists() and last.exists() and hashlib.md5(current).hexdigest() in last.read_text().split()
        pristine = keep.read_bytes() if ours else current  # not ours: a new write put a clean file there

        def put(out_bytes, tmp=None, rel=rel, target=target, keep=keep, last=last, current=current, ours=ours):
            """Writes out_bytes over the file in --out: the clean copy and the note first."""
            if not ours:
                keep.parent.mkdir(parents=True, exist_ok=True)
                keep.write_bytes(current)
            last.write_text(f"{hashlib.md5(current).hexdigest()}\n{hashlib.md5(out_bytes).hexdigest()}\n")
            if tmp is None:
                tmp = args.out / (rel + ".tmp")
                tmp.write_bytes(out_bytes)
            _detail_replace(tmp, target)

        why = None
        if rel.lower() in skip_rels:
            why = "cut-out"
        elif detail_skip_reason(rel):
            why = detail_skip_reason(rel)
        elif not src.exists():
            why = "no original"
        elif src.read_bytes() in skip_bytes:
            why = "same bytes as a cut-out"
        if why is None:
            o_img = hdt.read_texture(src)
            h_img = hdt.read_texture(keep if ours else target)
            if h_img.size != (o_img.size[0] * SCALE, o_img.size[1] * SCALE):
                why = f"HD {h_img.size[0]}x{h_img.size[1]} is not x{SCALE} of {o_img.size[0]}x{o_img.size[1]}"
        if why:
            count["skipped"] += 1
            if pristine != current:  # it got grain from an earlier run: the clean file back
                count["restored"] += 1
                why += ", clean file put back" + (" (dry run: not)" if args.dry_run else "")
                if not args.dry_run:
                    put(pristine)
            print(f"skip {rel}: {why}")
            continue
        t0 = time.time()
        wrap = detail_wrap(rel, repeats)
        o = np.asarray(o_img.convert("RGB"), dtype=np.float32) / 255
        hd = np.asarray(h_img.convert("RGB"), dtype=np.float32) / 255
        new, ratio, k = add_detail(o, hd, wrap)
        flat = ratio < DETAIL_TARGET and new is None  # below the target only when the original is flat
        out_img = None
        if new is None:
            ratio_after = ratio
        else:
            out_img = Image.fromarray((new * 255 + 0.5).astype(np.uint8), "RGB")
            ratio_after = detail_ratio(o, np.asarray(out_img, dtype=np.float32) / 255, wrap)[0]  # before JPEG
        if not flat:
            before.append(ratio)
            after.append(ratio_after)
        count["changed"] += new is not None
        state = "changed" if new is not None else ("flat" if flat else "kept")
        note = ""
        if not args.dry_run:
            if out_img is None:
                if pristine != current:  # no longer needs grain: the clean file back
                    put(pristine)
                    count["restored"] += 1
                    note = ", clean file put back"
            else:
                tmp = args.out / (rel + ".tmp")
                hdt.write_ozj(tmp, out_img)
                out_bytes = tmp.read_bytes()
                if out_bytes == current:
                    os.remove(tmp)
                    note = ", up to date"
                else:
                    put(out_bytes, tmp)
                    count["written"] += 1
                    note = ", written"
        print(f"{rel}: ratio {ratio:.2f} -> {ratio_after:.2f} k {k:.2f} {state}{note} "
              f"wrap {'x' if wrap[0] else '-'}{'y' if wrap[1] else '-'} {(time.time() - t0) * 1000:.0f} ms", flush=True)
    med = (lambda v: f"{float(np.median(v)):.2f}" if v else "-")
    c = count
    done = (" (dry run, nothing written)" if args.dry_run else
            f" ({c['written']} written, {c['changed'] - c['written']} already up to date)")
    print(f"{c['changed']} of {len(rels) - c['skipped']} textures changed{done}, {c['restored']} put back clean, "
          f"{c['skipped']} skipped; median ratio {med(before)} -> {med(after)} "
          f"(flat originals left out); {time.time() - started:.0f} s")


def detail_selftest() -> int:
    """Checks add_detail on synthetic textures: the ratio comes up toward the target (at least half the
    gap: the bicubic x4 loses part of the added band when boxed down again) and not past it, a texture
    that already keeps its detail is left alone, and a wrapped tile stays seamless."""
    rng = np.random.default_rng(1)
    n = 64
    o = gblur(rng.random((n, n, 3)).astype(np.float32), 0.7, (True, True)) * 0.6 + 0.2
    sharp = np.repeat(np.repeat(o, SCALE, 0), SCALE, 1)  # boxes back to the original exactly
    soft = gblur(sharp, 2.5 * SCALE, (True, True))  # the band smoothed away
    ok = True
    new, ratio, k = add_detail(o, soft, (True, True))
    ratio_new = detail_ratio(o, new, (True, True))[0] if new is not None else ratio
    print(f"soft: ratio {ratio:.2f} -> {ratio_new:.2f} k {k:.2f}")
    ok &= new is not None and ratio < 0.5 and ratio_new - ratio >= 0.5 * (DETAIL_TARGET - ratio)
    ok &= ratio_new <= DETAIL_TARGET + 0.02
    kept, ratio_sharp, _ = add_detail(o, sharp, (True, True))
    print(f"sharp: ratio {ratio_sharp:.2f}, {'kept' if kept is None else 'CHANGED'}")
    ok &= kept is None and ratio_sharp >= DETAIL_TARGET
    seam = float(np.abs(new[:, 0] - new[:, -1]).mean())
    seam_y = float(np.abs(new[0] - new[-1]).mean())
    # neighbours inside at the same place in a 4 px block as the seam (column 4m-1 next to 4m)
    inner = float(np.abs(new[:, SCALE::SCALE] - new[:, SCALE - 1:-1:SCALE]).mean())
    print(f"seams x {seam:.4f} y {seam_y:.4f}, neighbours inside {inner:.4f}")
    ok &= seam <= inner * 1.2 and seam_y <= inner * 1.2
    ok &= detail_selftest_runs(o, soft)
    print("detail self-check", "OK" if ok else "FAILED")
    return 0 if ok else 1


def detail_selftest_runs(o, soft) -> bool:
    """cmd_detail on files in a temporary folder: a run stopped between the note and the file, then run
    again, keeps the clean copy and gives what a clean run gives; its stale .tmp goes; a file that is
    skipped later gets its clean file back."""
    import contextlib
    import io
    import shutil
    import tempfile
    global DETAIL_TARGET, DETAIL_SKIP, _detail_replace
    saved = DETAIL_TARGET, DETAIL_SKIP, _detail_replace
    root = Path(tempfile.mkdtemp(prefix="hdart-detail-"))
    rel = "World1/TileTest.OZJ"

    class Stop(Exception):
        pass

    def stop(*_):
        raise Stop

    def make(name):
        for sub, a in (("src", o), ("out", soft)):
            (root / name / sub / "World1").mkdir(parents=True)
            hdt.write_ozj(root / name / sub / rel, Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGB"))
        return argparse.Namespace(src=root / name / "src", work=root / name / "work", out=root / name / "out",
                                  only=None, dry_run=False)

    def run(a):
        with contextlib.redirect_stdout(io.StringIO()):
            cmd_detail(a)

    ok = True
    try:
        clean = make("clean")
        DETAIL_TARGET = 0.80
        run(clean)
        want = (clean.out / rel).read_bytes()
        a = make("stopped")
        start = (a.out / rel).read_bytes()
        DETAIL_TARGET = 0.85
        run(a)  # grain at 0.85
        DETAIL_TARGET = 0.80
        _detail_replace = stop
        try:
            run(a)  # a new result at 0.80, stopped after the note, before the file
        except Stop:
            pass
        _detail_replace = saved[2]
        stale = (a.out / (rel + ".tmp")).exists()
        run(a)
        got = (a.out / rel).read_bytes()
        keep_ok = (a.work / "detail" / rel).read_bytes() == start
        tmp_gone = not (a.out / (rel + ".tmp")).exists()
        print(f"stopped run: stale tmp {stale} -> removed {tmp_gone}, clean copy kept {keep_ok}, "
              f"same as a clean run {got == want}")
        ok &= stale and tmp_gone and keep_ok and got == want and got != start
        DETAIL_SKIP = re.compile(r"tiletest", re.IGNORECASE)
        run(a)
        back = (a.out / rel).read_bytes() == start
        print(f"skipped later: clean file back {back}")
        ok &= back
    finally:
        DETAIL_TARGET, DETAIL_SKIP, _detail_replace = saved
        shutil.rmtree(root, ignore_errors=True)
    return ok


def main() -> int:
    if sys.argv[1:2] == ["detail-selftest"]:
        return detail_selftest()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("stage", choices=["plan", "esr", "sheets", "paint", "write", "cutouts", "detail"])
    ap.add_argument("--src", type=Path, required=True)
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--items", type=Path, help="Item textures to take (the rest are weapons and armour)")
    ap.add_argument("--gear", action="store_true", help="plan: equipment pass (Player, Item, Skill skins)")
    ap.add_argument("--skip-plan", type=Path, help="plan --gear: the first pass's plan.json, its textures are left out")
    ap.add_argument("--esrgan", type=Path)
    ap.add_argument("--sd", type=Path)
    ap.add_argument("--models", type=Path)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--only", nargs="*", help="write, detail: only these path patterns")
    ap.add_argument("--force", action="store_true", help="write: overwrite files already in --out")
    ap.add_argument("--png", action="store_true", help="write: also save PNG previews in <work>/preview")
    ap.add_argument("--dry-run", action="store_true", help="detail: print the ratios, write nothing")
    ap.add_argument("--allow-unpainted", action="store_true", help="write: upscale-only for textures not painted")
    args = ap.parse_args()
    if args.stage == "detail" and args.out is None:
        ap.error("detail needs --out (the Data/HD folder to work on)")
    {"plan": cmd_plan, "esr": cmd_esr, "sheets": cmd_sheets, "paint": cmd_paint, "write": cmd_write,
     "cutouts": cmd_cutouts, "detail": cmd_detail}[args.stage](args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
