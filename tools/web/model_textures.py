"""The texture files a model (.bmd) names, read from the model itself (ZzzBMD.cpp BMD::Open2).

    python -I tools/web/model_textures.py <model.bmd> ...
"""
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

HEADER = b"BMD"
VERSION_PLAIN = 0x0A
VERSION_ENCRYPTED = 0x0C
NAME_BYTES = 32
# Bytes of a vertex, a normal, a texture coordinate and a triangle in the file (ZzzBMD.h Vertex_t,
# Normal_t, TexCoord_t, Triangle_t2).
VERTEX_BYTES, NORMAL_BYTES, TEXCOORD_BYTES, TRIANGLE_BYTES = 16, 20, 8, 64


def texture_names(path, decrypt):
    """The names the model's meshes give their textures ("skin.jpg"), in the model's order, without
    repeats; [] for a file that isn't a model this reads. `decrypt` is make_startup_pack.py's
    decrypt_map_file."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:3] != HEADER or len(data) < 8:
        return []
    if data[3] == VERSION_ENCRYPTED:
        size = struct.unpack_from("<i", data, 4)[0]
        data = decrypt(data[8:8 + size])
        at = 0
    elif data[3] == VERSION_PLAIN:
        at = 4
    else:
        return []
    names = []
    try:
        meshes = struct.unpack_from("<h", data, at + NAME_BYTES)[0]
        at += NAME_BYTES + 6
        for _ in range(meshes):
            vertices, normals, texcoords, triangles, _texture = struct.unpack_from("<5h", data, at)
            at += 10 + vertices * VERTEX_BYTES + normals * NORMAL_BYTES + texcoords * TEXCOORD_BYTES
            at += triangles * TRIANGLE_BYTES
            raw = data[at:at + NAME_BYTES]
            if len(raw) < NAME_BYTES:
                return names
            at += NAME_BYTES
            name = raw.split(b"\0", 1)[0].decode("latin-1")
            if name and name not in names:
                names.append(name)
    except struct.error:
        pass
    return names


if __name__ == "__main__":
    import make_startup_pack

    for model in sys.argv[1:]:
        print(model, " ".join(texture_names(model, make_startup_pack.decrypt_map_file)))
