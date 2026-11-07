"""Builds the probe of the HD model pack: new models for Jewel of Bless (Jewel01), Jewel of Soul
(Jewel02) and the large healing potion (Potion04), with their own textures.

    python -I make_probe.py <Data folder of the client>

Reads the original models from <Data>\\Item and writes the new ones and their textures to
<Data>\\HD\\Item, where the client loads them with HD textures on (BMD::Open2,
Render::Textures::Hd::Resolve). Each new model keeps the original's meshes, bone, actions and
size, so the item's inventory placement and its effects stay as they are; the textures have new
names, because the originals are shared with other items.
"""
import io
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

# python -I leaves this folder off the path.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmd  # noqa: E402

TEXTURE_SIZE = 256


# ------------------------------------------------------------------ geometry helpers

def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _normalize(a):
    length = math.sqrt(_dot(a, a)) or 1.0
    return (a[0] / length, a[1] / length, a[2] / length)


class MeshBuilder:
    """Collects triangles for one BMD mesh, every corner pointing at its own normal and UV."""

    def __init__(self, texture):
        self.mesh = bmd.Mesh(texture=texture)
        self._vertex_ids = {}

    def vertex(self, p):
        key = tuple(round(c, 4) for c in p)
        if key not in self._vertex_ids:
            self._vertex_ids[key] = len(self.mesh.vertices)
            self.mesh.vertices.append((0, key))
        return self._vertex_ids[key]

    def triangle(self, points, normals, uvs, outward):
        """Adds a triangle, wound counter-clockwise seen from `outward` (as the originals are)."""
        face = _cross(_sub(points[1], points[0]), _sub(points[2], points[0]))
        if _dot(face, outward) < 0:
            points = [points[0], points[2], points[1]]
            normals = [normals[0], normals[2], normals[1]]
            uvs = [uvs[0], uvs[2], uvs[1]]
        vi, ni, ti = [], [], []
        for p, n, t in zip(points, normals, uvs):
            v = self.vertex(p)
            vi.append(v)
            ni.append(len(self.mesh.normals))
            self.mesh.normals.append((0, _normalize(n), v))
            ti.append(len(self.mesh.uvs))
            self.mesh.uvs.append(t)
        self.mesh.triangles.append((tuple(vi), tuple(ni), tuple(ti)))

    def flat(self, points, uvs, outward):
        face = _normalize(_cross(_sub(points[1], points[0]), _sub(points[2], points[0])))
        if _dot(face, outward) < 0:
            face = (-face[0], -face[1], -face[2])
        self.triangle(points, [face] * 3, uvs, outward)


# ------------------------------------------------------------------ cut gems

class TileAtlas:
    """A texture of 8 x 8 tiles; each facet of a gem takes one tile, so facets differ in tone."""

    GRID = 8

    def __init__(self, seed):
        self.random = random.Random(seed)
        self.tones = {}

    def uvs(self, tile, corners):
        size = 1.0 / self.GRID
        column, row = tile % self.GRID, tile // self.GRID
        inset = 0.12
        return [((column + inset + c[0] * (1 - 2 * inset)) * size, (row + inset + c[1] * (1 - 2 * inset)) * size)
                for c in corners]

    def pick(self, group):
        """A tile for a facet of `group` (rows of the cut share a range of tones)."""
        low, high = {'table': (40, 64), 'crown': (16, 48), 'girdle': (8, 24), 'pavilion': (0, 32)}[group]
        return self.random.randrange(low, high)


def _gem_texture(base, seed):
    """Tiles from deep to bright: row 0 darkest (pavilion), row 7 brightest (table)."""
    rng = random.Random(seed)
    size = TEXTURE_SIZE
    tile = size // TileAtlas.GRID
    image = Image.new('RGB', (size, size))
    pixels = image.load()
    for row in range(TileAtlas.GRID):
        for column in range(TileAtlas.GRID):
            tone = 0.78 + 0.5 * (row / (TileAtlas.GRID - 1)) + rng.uniform(-0.14, 0.14)
            angle = rng.uniform(0, math.tau)
            dx, dy = math.cos(angle), math.sin(angle)
            sparkle = rng.random() < 0.22
            sx, sy = rng.uniform(0.3, 0.7), rng.uniform(0.3, 0.7)
            for y in range(tile):
                for x in range(tile):
                    fx, fy = x / (tile - 1), y / (tile - 1)
                    t = tone * (0.82 + 0.36 * ((fx - 0.5) * dx + (fy - 0.5) * dy + 0.5))
                    # Bright faces lean to white, dark ones keep the colour deep.
                    white = max(0.0, t - 1.05) * 0.4
                    c = [min(255.0, ch * min(t, 1.0) + (255 - ch) * white) for ch in base]
                    if sparkle:
                        d = math.hypot(fx - sx, fy - sy)
                        glow = max(0.0, 1.0 - d / 0.32) ** 3 * 0.85
                        c = [ch + (255 - ch) * glow for ch in c]
                    pixels[column * tile + x, row * tile + y] = tuple(int(ch) for ch in c)
    return image


def _ring(count, radius, z, offset):
    return [(radius * math.cos(math.tau * (i + offset) / count), radius * math.sin(math.tau * (i + offset) / count), z)
            for i in range(count)]


def _cut_gem(texture, rings, atlas, group_of_band):
    """A convex gem from rings (count, radius, z, offset) bottom to top; radius 0 is a point."""
    builder = MeshBuilder(texture)
    count = rings[0][0]
    z_mid = (rings[0][2] + rings[-1][2]) / 2
    points = [_ring(count, r, z, o) if r > 0 else [(0.0, 0.0, z)] for _, r, z, o in rings]
    for band in range(len(points) - 1):
        lower, upper = points[band], points[band + 1]
        group = group_of_band[band]
        for i in range(count):
            quads = []
            if len(lower) == 1:
                quads.append([lower[0], upper[i], upper[(i + 1) % count]])
            elif len(upper) == 1:
                quads.append([lower[i], lower[(i + 1) % count], upper[0]])
            elif rings[band][3] == rings[band + 1][3]:
                quads.append([lower[i], lower[(i + 1) % count], upper[(i + 1) % count], upper[i]])
            else:
                # Upper ring is turned by half a step: kite-shaped facets meet in triangles.
                quads.append([lower[i], lower[(i + 1) % count], upper[i]])
                quads.append([upper[i], lower[(i + 1) % count], upper[(i + 1) % count]])
            for facet in quads:
                centre = tuple(sum(p[k] for p in facet) / len(facet) for k in range(3))
                outward = _sub(centre, (0.0, 0.0, z_mid))
                tile = atlas.pick(group)
                if len(facet) == 3:
                    builder.flat(facet, atlas.uvs(tile, [(0, 1), (1, 1), (0.5, 0)]), outward)
                else:
                    a = atlas.uvs(tile, [(0, 1), (1, 1), (1, 0), (0, 0)])
                    builder.flat([facet[0], facet[1], facet[2]], [a[0], a[1], a[2]], outward)
                    builder.flat([facet[0], facet[2], facet[3]], [a[0], a[2], a[3]], outward)
    top = rings[-1]
    if top[1] > 0:
        # A flat table, split from its centre.
        centre = (0.0, 0.0, top[2])
        for i in range(count):
            tile = atlas.pick('table')
            facet = [centre, points[-1][i], points[-1][(i + 1) % count]]
            builder.flat(facet, atlas.uvs(tile, [(0.5, 0.5), (0, 0), (1, 0)]), (0.0, 0.0, 1.0))
    return builder.mesh


def make_bless():
    atlas = TileAtlas(seed=11)
    rings = [
        (10, 0.0, 7.6, 0.0),     # culet
        (10, 6.8, 15.5, 0.5),    # pavilion facets
        (10, 10.1, 23.4, 0.0),   # girdle
        (10, 10.1, 24.3, 0.0),
        (10, 7.4, 28.6, 0.5),    # crown facets
        (10, 4.0, 32.0, 0.0),    # table
    ]
    groups = ['pavilion', 'pavilion', 'girdle', 'crown', 'crown']
    mesh = _cut_gem('hd_bless.jpg', rings, atlas, groups)
    return [mesh], {'hd_bless': _gem_texture((125, 115, 255), seed=21)}


def make_soul():
    atlas = TileAtlas(seed=12)
    rings = [
        (16, 0.0, 0.1, 0.0),     # culet
        (16, 6.4, 8.6, 0.5),     # lower pavilion
        (16, 11.2, 16.0, 0.0),   # girdle
        (16, 11.2, 16.9, 0.0),
        (16, 9.2, 19.6, 0.5),    # star facets
        (16, 6.4, 22.0, 0.0),    # table
    ]
    groups = ['pavilion', 'pavilion', 'girdle', 'crown', 'crown']
    mesh = _cut_gem('hd_soul.jpg', rings, atlas, groups)
    return [mesh], {'hd_soul': _gem_texture((255, 75, 195), seed=22)}


# ------------------------------------------------------------------ potion

SEGMENTS = 28


def _revolve(builder, centre, profile, u_range, v_of):
    """A smooth surface of revolution; `profile` is (radius, z) points, bottom to top."""
    cx, cy = centre

    def normal_2d(k):
        # Outward normal of the profile at point k: (dz, -dr), averaged over its neighbours.
        before = profile[max(k - 1, 0)]
        after = profile[min(k + 1, len(profile) - 1)]
        dr, dz = after[0] - before[0], after[1] - before[1]
        length = math.hypot(dr, dz) or 1.0
        return dz / length, -dr / length

    for k in range(len(profile) - 1):
        (r0, z0), (r1, z1) = profile[k], profile[k + 1]
        n0, n1 = normal_2d(k), normal_2d(k + 1)
        for i in range(SEGMENTS):
            a0 = math.tau * i / SEGMENTS
            a1 = math.tau * (i + 1) / SEGMENTS
            u0 = u_range[0] + (u_range[1] - u_range[0]) * i / SEGMENTS
            u1 = u_range[0] + (u_range[1] - u_range[0]) * (i + 1) / SEGMENTS

            def point(r, z, a):
                return (cx + r * math.cos(a), cy + r * math.sin(a), z)

            def normal(n, a):
                return (n[0] * math.cos(a), n[0] * math.sin(a), n[1])

            corners = [(r0, z0, a0, n0, u0), (r0, z0, a1, n0, u1), (r1, z1, a1, n1, u1), (r1, z1, a0, n1, u0)]
            # The outward side of this patch, from the profile's own direction.
            mid_a = (a0 + a1) / 2
            dr, dz = r1 - r0, z1 - z0
            outward = (dz * math.cos(mid_a), dz * math.sin(mid_a), -dr)
            quad = [(point(r, z, a), normal(n, a), (u, v_of(z))) for r, z, a, n, u in corners]
            for tri in ((0, 1, 2), (0, 2, 3)):
                pts = [quad[j][0] for j in tri]
                if len({tuple(round(c, 4) for c in p) for p in pts}) < 3:
                    continue  # the point at the centre of a disc
                builder.triangle(pts, [quad[j][1] for j in tri], [quad[j][2] for j in tri], outward)


def _potion_glass_texture():
    size = TEXTURE_SIZE
    image = Image.new('RGB', (size, size))
    pixels = image.load()
    liquid_top = 1 - 29.0 / 33.1  # v of the liquid's surface
    for y in range(size):
        v = y / (size - 1)
        for x in range(size):
            u = x / (size - 1)
            # Light comes through the side facing the viewer's upper left.
            side = 0.5 + 0.5 * math.cos(math.tau * (u - 0.62))
            if v >= liquid_top:
                depth = (v - liquid_top) / (1 - liquid_top)
                r = 120 + 125 * side - 40 * depth
                g = 6 + 30 * side ** 3
                b = 10 + 22 * side ** 3
            else:
                # Empty glass above the liquid: a pale, slightly red tint.
                r = 90 + 60 * side
                g = 40 + 40 * side
                b = 45 + 40 * side
            pixels[x, y] = (int(max(0, min(255, r))), int(max(0, min(255, g))), int(max(0, min(255, b))))
    draw = ImageDraw.Draw(image)
    surface = int(liquid_top * (size - 1))
    draw.rectangle([0, surface - 2, size, surface + 2], fill=(255, 120, 110))
    # A soft white streak where the glass catches the light.
    streak = Image.new('L', (size, size), 0)
    ImageDraw.Draw(streak).rectangle([int(0.56 * size), int(0.12 * size), int(0.6 * size), int(0.9 * size)], fill=150)
    streak = streak.filter(ImageFilter.GaussianBlur(4))
    image = Image.composite(Image.new('RGB', (size, size), (255, 255, 255)), image, streak)
    return image


def _potion_trim_texture():
    """Left half brass bands, right half cork."""
    size = TEXTURE_SIZE
    rng = random.Random(5)
    image = Image.new('RGB', (size, size))
    pixels = image.load()
    for y in range(size):
        v = y / (size - 1)
        for x in range(size):
            if x < size // 2:
                shine = 0.55 + 0.45 * math.sin(v * math.pi * 3) ** 2
                pixels[x, y] = (int(205 * shine + 40), int(150 * shine + 25), int(55 * shine + 10))
            else:
                grain = rng.uniform(-14, 14)
                pixels[x, y] = (int(150 + grain), int(104 + grain), int(64 + grain * 0.6))
    return image.filter(ImageFilter.SMOOTH)


def make_potion(original):
    glass_original, trim_original = original.meshes
    xs = [p[1][0] for p in glass_original.vertices]
    ys = [p[1][1] for p in glass_original.vertices]
    centre = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2)

    def v_of(z):
        return min(1.0, max(0.0, 1 - z / 33.1))

    glass = MeshBuilder('hd_potion_red.jpg')
    body = [(0.0, 0.0), (6.8, 0.0), (8.6, 0.5), (9.5, 2.0), (9.9, 6.0), (9.9, 13.0), (9.6, 19.5), (8.9, 23.5),
            (7.2, 26.6), (5.0, 29.0), (3.9, 30.6), (3.8, 31.6), (4.5, 32.2), (4.6, 33.1)]
    _revolve(glass, centre, body, (0.0, 1.0), v_of)

    trim = MeshBuilder('hd_potion_trim.jpg')
    brass = (0.0, 0.5)
    for low in (3.4, 17.0):
        band = [(9.6, low), (10.3, low + 0.4), (10.6, low + 1.2), (10.6, low + 2.2), (10.3, low + 3.0),
                (9.6, low + 3.4)]
        _revolve(trim, centre, band, brass, lambda z, low=low: (z - low) / 3.4 * 0.9 + 0.05)
    cork = [(3.5, 31.0), (4.1, 32.6), (4.4, 34.8), (4.3, 36.4), (3.9, 37.0)]
    _revolve(trim, centre, cork, (0.5, 1.0), lambda z: (37.2 - z) / 6.2)
    cork_top = [(3.9, 37.0), (2.4, 37.25), (0.0, 37.3)]
    _revolve(trim, centre, cork_top, (0.5, 1.0), lambda z: 0.02)

    return [glass.mesh, trim.mesh], {'hd_potion_red': _potion_glass_texture(), 'hd_potion_trim': _potion_trim_texture()}


# ------------------------------------------------------------------ files

def write_ozj(image, path):
    """An OZJ: a 24-byte header the client skips, then the JPEG."""
    data = io.BytesIO()
    image.save(data, 'JPEG', quality=93, subsampling=0)
    with open(path, 'wb') as f:
        f.write(bytes(24) + data.getvalue())


def main():
    data_folder = sys.argv[1]
    source = os.path.join(data_folder, 'Item')
    target = os.path.join(data_folder, 'HD', 'Item')
    os.makedirs(target, exist_ok=True)
    jobs = {
        'Jewel01': lambda model: make_bless(),
        'Jewel02': lambda model: make_soul(),
        'Potion04': make_potion,
    }
    for name, make in jobs.items():
        original = bmd.read(os.path.join(source, name + '.bmd'))
        meshes, textures = make(original)
        assert len(meshes) == len(original.meshes), name
        for index, mesh in enumerate(meshes):
            mesh.texture_index = original.meshes[index].texture_index
        model = bmd.Model(original.name, meshes, original.num_bones, original.num_actions, original.tail)
        bmd.write(model, os.path.join(target, name + '.bmd'))
        for texture_name, image in textures.items():
            write_ozj(image, os.path.join(target, texture_name + '.OZJ'))
        print(name, [(m.texture, len(m.vertices), len(m.triangles)) for m in meshes])


if __name__ == '__main__':
    main()
