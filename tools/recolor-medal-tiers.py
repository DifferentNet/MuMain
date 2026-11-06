#!/usr/bin/env python3
"""Makes the textures and model copies of the tiered medals (14:170-177).

Silver Medal, Gold Medal, Firecracker and Heart of Love (Box of Luck +5, +6, +2, +3) get two higher
tiers: tier 2 in sapphire blue, tier 3 in crimson red. Their textures (medal1, medal2, firecracker,
hhj) are recoloured copies with the same file names in Data/Item/MedalTier2 and Data/Item/MedalTier3;
the item models (Data/Items/Models/Group14_Potion.json) list that folder before "Item", so each
texture is taken from there. Each item also has a copy of its model file in the folder: two items
may only share a model file when they share its textures (test_item_model_files.cpp).

    python -I tools/recolor-medal-tiers.py [--client-data src/bin/Data]

Needs Pillow and numpy. The result is the same on every run. The colour is made in HSV: the hue of the
texture is pulled to the tier's hue (keeping 40 % of its variation where it is coloured, so stripes and symbols stay
apart), the saturation is raised so the grey ground takes the colour too, and the brightness is
scaled. OZJ is a 24-byte header followed by a JPEG; the header of the original is kept.
"""
import argparse
import io
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HEADER_SIZE = 24
TEXTURES = ("medal1", "medal2", "firecracker", "hhj")
# item model file of the originals, copied into the tier folders
MODELS = ("MagicBox03", "MagicBox05", "MagicBox06", "MagicBox07")
JPEG_QUALITY = 95
HUE_KEEP = 0.4


class Tier:
    def __init__(self, folder, hue_degrees, saturation_floor, brightness):
        self.folder = folder
        self.hue = hue_degrees / 360.0
        self.saturation_floor = saturation_floor
        self.brightness = brightness


TIERS = (
    Tier("MedalTier2", 215, 0.50, 0.95),  # sapphire
    Tier("MedalTier3", 348, 0.60, 0.72),  # crimson, darker so it differs from the plain red originals
)


def read_ozj(path):
    data = path.read_bytes()
    return data[:HEADER_SIZE], Image.open(io.BytesIO(data[HEADER_SIZE:])).convert("RGB")


def recolor(image, tier):
    hsv = np.asarray(image.convert("HSV"), dtype=np.float64) / 255.0
    hue, sat, val = hsv[..., 0], hsv[..., 1], hsv[..., 2]

    # the hue most of the colour is in (circular mean weighted by saturation and brightness)
    weight = sat * val
    angle = hue * 2 * np.pi
    mean = np.arctan2((np.sin(angle) * weight).sum(), (np.cos(angle) * weight).sum()) / (2 * np.pi) % 1.0
    delta = (hue - mean + 0.5) % 1.0 - 0.5
    # grey pixels have no real hue (it is noise): they take the tier hue as it is
    colourful = np.clip(sat * 4.0, 0.0, 1.0)
    new_hue = (tier.hue + delta * HUE_KEEP * colourful) % 1.0

    new_sat = tier.saturation_floor + (1.0 - tier.saturation_floor) * sat
    new_val = np.clip(val * tier.brightness, 0.0, 1.0)

    out = np.stack([new_hue, new_sat, new_val], axis=-1)
    return Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), "HSV").convert("RGB")


def write_ozj(path, header, image):
    buffer = io.BytesIO()
    image.save(buffer, "JPEG", quality=JPEG_QUALITY, subsampling=0, optimize=False)
    path.write_bytes(header + buffer.getvalue())


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--client-data", default="src/bin/Data", type=Path)
    args = parser.parse_args()
    item_dir = args.client_data / "Item"

    for tier in TIERS:
        out_dir = item_dir / tier.folder
        out_dir.mkdir(exist_ok=True)
        for name in TEXTURES:
            header, image = read_ozj(item_dir / f"{name}.OZJ")
            write_ozj(out_dir / f"{name}.OZJ", header, recolor(image, tier))
        for name in MODELS:
            shutil.copyfile(item_dir / f"{name}.bmd", out_dir / f"{name}.bmd")
        print(f"{out_dir}: {len(TEXTURES)} textures, {len(MODELS)} models")


if __name__ == "__main__":
    sys.exit(main())
