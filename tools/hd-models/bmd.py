"""Reads and writes MU BMD models (versions 0xA and 0xC) for the HD model pack.

The layout is the one BMD::Open2 (src/source/Render/Models/ZzzBMD.cpp) reads: a 32-byte name,
mesh, bone and action counts, then per mesh its vertices (16 bytes), normals (20), texture
coordinates (8), triangles (64 bytes, Triangle_t2) and a 32-byte texture name, then the actions
and bones, kept here as raw bytes.
"""
import struct
from dataclasses import dataclass, field

_XOR = [0xD1, 0x73, 0x52, 0xF6, 0xD2, 0x9A, 0xCB, 0x27, 0x3E, 0xAF, 0x59, 0x31, 0x37, 0xB3, 0xE7, 0xA2]


def _decrypt(data):
    out = bytearray(len(data))
    key = 0x5E
    for i, c in enumerate(data):
        out[i] = ((c ^ _XOR[i % 16]) - key) & 0xFF
        key = (c + 0x3D) & 0xFF
    return bytes(out)


@dataclass
class Mesh:
    texture: str
    vertices: list = field(default_factory=list)   # (node, (x, y, z))
    normals: list = field(default_factory=list)    # (node, (x, y, z), bind vertex)
    uvs: list = field(default_factory=list)        # (u, v)
    triangles: list = field(default_factory=list)  # ((v0, v1, v2), (n0, n1, n2), (t0, t1, t2))
    texture_index: int = 0


@dataclass
class Model:
    name: bytes
    meshes: list
    num_bones: int
    num_actions: int
    tail: bytes  # actions and bones, as stored


def read(path):
    raw = open(path, 'rb').read()
    assert raw[:3] == b'BMD', path
    version = raw[3]
    if version == 0x0C:
        size = struct.unpack_from('<i', raw, 4)[0]
        data = _decrypt(raw[8:8 + size])
    elif version == 0x0A:
        data = raw[4:]
    else:
        raise ValueError(f'{path}: BMD version {version:#x}')
    o = 0
    name = data[o:o + 32]; o += 32
    num_meshes, num_bones, num_actions = struct.unpack_from('<hhh', data, o); o += 6
    meshes = []
    for _ in range(num_meshes):
        nv, nn, nt, ntri, tex = struct.unpack_from('<hhhhh', data, o); o += 10
        m = Mesh(texture='', texture_index=tex)
        for _ in range(nv):
            node, _pad, x, y, z = struct.unpack_from('<hhfff', data, o); o += 16
            m.vertices.append((node, (x, y, z)))
        for _ in range(nn):
            node, _pad, x, y, z, bind, _pad2 = struct.unpack_from('<hhfffhh', data, o); o += 20
            m.normals.append((node, (x, y, z), bind))
        for _ in range(nt):
            u, v = struct.unpack_from('<ff', data, o); o += 8
            m.uvs.append((u, v))
        for _ in range(ntri):
            poly = data[o]
            vi = struct.unpack_from('<4h', data, o + 2)
            ni = struct.unpack_from('<4h', data, o + 10)
            ti = struct.unpack_from('<4h', data, o + 18)
            o += 64
            if poly != 3:
                raise ValueError(f'{path}: polygon with {poly} corners')
            m.triangles.append((vi[:3], ni[:3], ti[:3]))
        m.texture = data[o:o + 32].split(b'\0')[0].decode('latin1'); o += 32
        meshes.append(m)
    return Model(name, meshes, num_bones, num_actions, data[o:])


def write(model, path):
    out = bytearray()
    out += model.name[:32].ljust(32, b'\0')
    out += struct.pack('<hhh', len(model.meshes), model.num_bones, model.num_actions)
    for m in model.meshes:
        out += struct.pack('<hhhhh', len(m.vertices), len(m.normals), len(m.uvs), len(m.triangles),
                           m.texture_index)
        for node, (x, y, z) in m.vertices:
            out += struct.pack('<hhfff', node, 0, x, y, z)
        for node, (x, y, z), bind in m.normals:
            out += struct.pack('<hhfffhh', node, 0, x, y, z, bind, 0)
        for u, v in m.uvs:
            out += struct.pack('<ff', u, v)
        for vi, ni, ti in m.triangles:
            tri = bytearray(64)
            tri[0] = 3
            struct.pack_into('<4h', tri, 2, *vi, 0)
            struct.pack_into('<4h', tri, 10, *ni, 0)
            struct.pack_into('<4h', tri, 18, *ti, 0)
            out += tri
        out += m.texture.encode('latin1')[:31].ljust(32, b'\0')
    out += model.tail
    with open(path, 'wb') as f:
        f.write(b'BMD\x0a' + bytes(out))
